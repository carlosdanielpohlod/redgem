#include "project/game_build.hpp"

#include <cctype>

#include "app/bundled_game.hpp"
#include "app/scene_files.hpp"
#include "core/config.hpp"
#include "core/files.hpp"
#include "core/platform.hpp"
#include "project/android/android_package.hpp"
#include "project/notices.hpp"

namespace redgem {
namespace {

constexpr const char* kParentFolder = "..";          // how a path says "one folder up"
constexpr const char* kFallbackExecutable = "play";  // for a game whose name gives no usable file name

// What is built is a game, and not into itself.
bool can_build(const std::filesystem::path& game_dir, const std::filesystem::path& out_dir, std::string& error) {
  return is_game_to_build(game_dir, error) && builds_outside_game(game_dir, out_dir, error);
}

// The file a game's executable is, built for `target`.
std::string executable_file(const std::filesystem::path& game_dir, BuildTarget target) {
  return platform::executable_file_name(executable_name(game_dir), target == BuildTarget::Windows);
}

// Whether a name can be the executable's: there is one, and it is not the name of the folder the
// game's files go in beside it.
bool names_an_executable(const std::string& name) { return !name.empty() && name != kBundledGameFolder; }

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

std::optional<std::filesystem::path> build_game_for(const std::filesystem::path& game_dir,
                                                    const std::filesystem::path& out_dir, BuildTarget target,
                                                    std::string& error) {
  const std::filesystem::path self = platform::executable_path();
  if (target == BuildTarget::Android) {
    return build_android_game(game_dir, out_dir, runtime_location(target, self), error);
  }
  // (What is wrong with the game is said before what is missing here: it is the nearer mistake.)
  if (!can_build(normalized_folder(game_dir), normalized_folder(out_dir), error)) return std::nullopt;
  const auto runtime = runtime_for(target, self);
  if (!runtime) {
    error = missing_runtime_message(target, runtime_location(target, self));
    return std::nullopt;
  }
  if (!build_game(game_dir, out_dir, *runtime, target, error)) return std::nullopt;
  return normalized_folder(out_dir) / executable_file(game_dir, target);
}

namespace {

// The player as the game's executable, allowed to run.
bool copy_player(const std::filesystem::path& player, const std::filesystem::path& binary, std::string& error) {
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

// A fresh copy in `bundle` of the files of the game that ship (see shipped_files).
bool copy_game(const std::filesystem::path& game_dir, const std::filesystem::path& bundle, std::string& error) {
  std::error_code fs_error;
  std::filesystem::remove_all(bundle, fs_error);
  std::filesystem::create_directories(bundle, fs_error);
  for (const std::filesystem::path& file : shipped_files(game_dir)) {
    std::filesystem::create_directories((bundle / file).parent_path(), fs_error);
    std::filesystem::copy_file(game_dir / file, bundle / file, fs_error);
    if (!fs_error) continue;
    error = "cannot copy the game: " + file.generic_string() + ": " + fs_error.message();
    return false;
  }
  return true;
}

}  // namespace

bool build_game(const std::filesystem::path& game_folder, const std::filesystem::path& out_folder,
                const std::filesystem::path& player, BuildTarget target, std::string& error) {
  const std::filesystem::path game_dir = normalized_folder(game_folder);
  const std::filesystem::path out_dir = normalized_folder(out_folder);
  if (!can_build(game_dir, out_dir, error)) return false;
  std::error_code fs_error;
  std::filesystem::create_directories(out_dir, fs_error);
  const std::filesystem::path binary = out_dir / executable_file(game_dir, target);
  return copy_player(player, binary, error) && copy_game(game_dir, out_dir / kBundledGameFolder, error) &&
         write_notices(out_dir, error);
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
