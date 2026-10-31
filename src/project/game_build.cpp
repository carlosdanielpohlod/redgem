#include "project/game_build.hpp"

#include <cctype>
#include <string>
#include <vector>

#include "app/bundled_game.hpp"
#include "app/scene_files.hpp"
#include "core/config.hpp"
#include "core/files.hpp"
#include "core/platform.hpp"
#include "core/translations.hpp"
#include "project/android/android_package.hpp"
#include "project/build_record.hpp"
#include "project/build_steps.hpp"
#include "project/game_shipping.hpp"
#include "project/notices.hpp"

namespace redgem {
namespace {

constexpr const char* kParentFolder = "..";          // how a path says "one folder up"
constexpr const char* kFallbackExecutable = "play";  // for a game whose name gives no usable file name

bool game_not_in_bundle(const std::filesystem::path& game_dir, const std::filesystem::path& out_dir,
                        std::string& error);

// What is built is a game, and not into itself, nor into a game folder that holds it (which a
// build for a computer replaces).
bool can_build(const std::filesystem::path& game_dir, const std::filesystem::path& out_dir, std::string& error) {
  return is_game_to_build(game_dir, error) && builds_outside_game(game_dir, out_dir, error) &&
         game_not_in_bundle(game_dir, out_dir, error);
}

// The file a game's executable is, built for `target`.
std::string executable_file(const std::filesystem::path& game_dir, BuildTarget target) {
  return platform::executable_file_name(executable_name(game_dir), target == BuildTarget::Windows);
}

// Whether a name can be the executable's: there is one, and it is not the name of the folder the
// game's files go in beside it.
bool names_an_executable(const std::string& name) { return !name.empty() && name != kBundledGameFolder; }

// Whether a computer's build can make what is asked for: an App Bundle is Google Play's.
bool fits_computer(AndroidFormat format, std::string& error) {
  if (format == AndroidFormat::Apk) return true;
  error = tr("project.build.bundle_is_for_android");
  return false;
}

// A Mac app built on a Mac is signed (ad hoc) before it is said to be made.
bool signed_if_mac_app(const std::filesystem::path& made, BuildTarget target, std::string& error) {
  if (target != BuildTarget::MacOS || !platform::is_mac()) return true;
  return sign_mac_app(made, error);
}

std::optional<std::filesystem::path> build_made(const std::filesystem::path& game_dir,
                                                const std::filesystem::path& out_dir,
                                                const std::filesystem::path& player, BuildTarget target,
                                                std::string& error);

}  // namespace

bool is_game_to_build(const std::filesystem::path& game_dir, std::string& error) {
  if (is_game_folder(game_dir)) return true;
  error = game_dir.string() + " is not a game (no " + config::kSceneFileName + ")";
  return false;
}

bool builds_outside_game(const std::filesystem::path& game_dir, const std::filesystem::path& out_dir,
                         std::string& error) {
  if (!folder_inside(out_dir, game_dir)) return true;
  error = "cannot build into " + out_dir.string() + ": it is inside the game folder";
  return false;
}

namespace {

// The game folder a build replaces must not be the game itself, nor hold it.
bool game_not_in_bundle(const std::filesystem::path& game_dir, const std::filesystem::path& out_dir,
                        std::string& error) {
  return game_outside(game_dir, out_dir / kBundledGameFolder, error);
}

}  // namespace

bool game_outside(const std::filesystem::path& game_dir, const std::filesystem::path& replaced, std::string& error) {
  if (!folder_inside(game_dir, replaced)) return true;
  error = tr("project.build.game_inside_its_build", {{"game", game_dir.string()}, {"folder", replaced.string()}});
  return false;
}

namespace {

// A build for a computer, with the player for `target` found beside this program (`self`).
std::optional<std::filesystem::path> build_for_computer(const std::filesystem::path& game_dir,
                                                        const std::filesystem::path& out_dir, BuildTarget target,
                                                        const std::filesystem::path& self, std::string& error) {
  // (What is wrong with the game is said before what is missing here: it is the nearer mistake.)
  if (!can_build(game_dir, out_dir, error)) return std::nullopt;
  const auto runtime = runtime_for(target, self);
  if (!runtime) {
    error = missing_runtime_message(target, runtime_location(target, self));
    return std::nullopt;
  }
  auto made = build_made(game_dir, out_dir, *runtime, target, error);
  if (!made || !signed_if_mac_app(*made, target, error)) return std::nullopt;
  return made;
}

}  // namespace

std::optional<std::filesystem::path> build_game_for(const std::filesystem::path& game_dir,
                                                    const std::filesystem::path& out_dir, BuildTarget target,
                                                    std::string& error, AndroidFormat format) {
  const std::filesystem::path self = platform::executable_path();
  if (target == BuildTarget::Android) {
    return build_android_game(game_dir, out_dir, runtime_location(target, self), error, format);
  }
  if (!fits_computer(format, error)) return std::nullopt;
  return build_for_computer(normalized_folder(game_dir), normalized_folder(out_dir), target, self, error);
}

bool copy_player(const std::filesystem::path& player, const std::filesystem::path& binary, std::string& error) {
  build_steps::begin(build_steps::kCopyingPlayer);
  std::error_code fs_error;
  std::filesystem::copy_file(player, binary, std::filesystem::copy_options::overwrite_existing, fs_error);
  if (fs_error) {
    error = "cannot copy the player " + player.string() + ": " + fs_error.message();
    return false;
  }
  std::filesystem::permissions(
      binary,
      std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec | std::filesystem::perms::others_exec,
      std::filesystem::perm_options::add, fs_error);
  return true;
}

// (An earlier build's files are emptied first: none of them may ship again.)
bool fresh_bundle(const std::filesystem::path& bundle, std::string& error) {
  std::error_code fs_error;
  std::filesystem::remove_all(bundle, fs_error);
  if (!fs_error) std::filesystem::create_directories(bundle, fs_error);
  if (!fs_error) return true;
  error = tr("project.build.cannot_empty", {{"folder", bundle.string()}, {"reason", fs_error.message()}});
  return false;
}

bool copy_game(const std::filesystem::path& game_dir, const std::filesystem::path& bundle, std::string& error) {
  build_steps::begin(build_steps::kCopyingFiles);
  if (!fresh_bundle(bundle, error)) return false;
  std::error_code fs_error;
  for (const std::filesystem::path& file : shipped_files(game_dir)) {
    std::filesystem::create_directories((bundle / file).parent_path(), fs_error);
    std::filesystem::copy_file(game_dir / file, bundle / file, fs_error);
    if (!fs_error) continue;
    error = "cannot copy the game: " + file.generic_string() + ": " + fs_error.message();
    return false;
  }
  return true;
}

namespace {

// A build for a computer other than the Mac: the executable beside the game's folder.
bool build_folder(const std::filesystem::path& game_dir, const std::filesystem::path& out_dir,
                  const std::filesystem::path& player, BuildTarget target, const BuildIdentity& identity,
                  std::string& error) {
  std::error_code fs_error;
  std::filesystem::create_directories(out_dir, fs_error);
  // (Only what a build made is written over: anything else of the same name is left as it is.)
  std::vector<std::string> outputs = {executable_file(game_dir, target), kBundledGameFolder, kNoticesFolder};
  const std::vector<std::string> shipping = shipping_outputs(game_dir, target);
  outputs.insert(outputs.end(), shipping.begin(), shipping.end());
  BuildRecord record = BuildRecord::of(out_dir);
  if (!record.may_write(outputs, error) || !record.keep(outputs, error)) return false;
  return copy_player(player, out_dir / outputs.front(), error) &&
         copy_game(game_dir, out_dir / kBundledGameFolder, error) && write_notices(out_dir, error) &&
         write_shipping_files(game_dir, out_dir, target, identity, error);
}

}  // namespace

namespace {

// Builds, and says what was made: the executable, or for the Mac the app.
std::optional<std::filesystem::path> build_made(const std::filesystem::path& game_dir,
                                                const std::filesystem::path& out_dir,
                                                const std::filesystem::path& player, BuildTarget target,
                                                std::string& error) {
  if (!can_build(game_dir, out_dir, error)) return std::nullopt;
  const auto identity = build_identity(game_dir, error);
  if (!identity) return std::nullopt;
  if (target == BuildTarget::MacOS) return build_mac_app(game_dir, out_dir, player, *identity, error);
  if (!build_folder(game_dir, out_dir, player, target, *identity, error)) return std::nullopt;
  return out_dir / executable_file(game_dir, target);
}

}  // namespace

bool build_game(const std::filesystem::path& game_folder, const std::filesystem::path& out_folder,
                const std::filesystem::path& player, BuildTarget target, std::string& error) {
  return build_made(normalized_folder(game_folder), normalized_folder(out_folder), player, target, error).has_value();
}

bool folder_inside(const std::filesystem::path& folder, const std::filesystem::path& outer) {
  std::error_code fs_error;
  const std::filesystem::path outer_real = std::filesystem::weakly_canonical(outer, fs_error);
  const std::filesystem::path folder_real = std::filesystem::weakly_canonical(folder, fs_error);
  const std::filesystem::path relative = folder_real.lexically_relative(outer_real);
  return !relative.empty() && *relative.begin() != kParentFolder;
}

std::string executable_name(const std::filesystem::path& game_dir) {
  std::string name;
  for (const char c : normalized_folder(game_dir).filename().string()) {
    const auto letter = static_cast<unsigned char>(c);
    name.push_back(std::isalnum(letter) != 0 ? static_cast<char>(std::tolower(letter)) : '_');
  }
  return names_an_executable(name) ? name : kFallbackExecutable;
}

}  // namespace redgem
