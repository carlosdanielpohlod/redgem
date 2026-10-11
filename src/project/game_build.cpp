#include "project/game_build.hpp"

#include <algorithm>
#include <cctype>

#include "app/bundled_game.hpp"
#include "core/config.hpp"
#include "core/files.hpp"
#include "core/platform.hpp"
#include "project/android_package.hpp"
#include "project/notices.hpp"

namespace redgem {
namespace {

constexpr const char* kLinuxTarget = "linux";
constexpr const char* kWindowsTarget = "windows";
constexpr const char* kAndroidTarget = "android";
constexpr const char* kSigningKeyExtensions[] = {".keystore", ".jks", ".p12"};
constexpr char kHiddenPrefix = '.';                  // of files and folders that are not part of a game (.git, .env)
constexpr char kBackupSuffix = '~';                  // of the copies editors leave behind
constexpr const char* kParentFolder = "..";          // how a path says "one folder up"
constexpr const char* kRuntimesFolder = "runtimes";  // engines for other systems, next to this one
constexpr const char* kRuntimeName = "redgem";
constexpr const char* kFallbackExecutable = "play";  // for a game whose name gives no usable file name

// Whether a file or folder of a game goes into a build of it. A copy of a game goes to other
// people: not what a link points at (that can be anything on the machine that builds), not hidden
// files and folders, not editors' backups, not a signing key left in the folder.
bool ships(const std::filesystem::directory_entry& entry) {
  std::error_code fs_error;
  const std::string name = entry.path().filename().string();
  const bool leftover = !name.empty() && (name.front() == kHiddenPrefix || name.back() == kBackupSuffix);
  return !entry.is_symlink(fs_error) && !leftover && !signing_key_file(entry.path());
}

}  // namespace

std::optional<BuildTarget> build_target_from_name(const std::string& name) {
  if (name == kLinuxTarget) return BuildTarget::Linux;
  if (name == kWindowsTarget) return BuildTarget::Windows;
  if (name == kAndroidTarget) return BuildTarget::Android;
  return std::nullopt;
}

const char* build_target_name(BuildTarget target) {
  switch (target) {
    case BuildTarget::Windows:
      return kWindowsTarget;
    case BuildTarget::Android:
      return kAndroidTarget;
    case BuildTarget::Linux:
      break;
  }
  return kLinuxTarget;
}

BuildTarget host_build_target() { return platform::is_windows() ? BuildTarget::Windows : BuildTarget::Linux; }

std::optional<std::filesystem::path> runtime_for(BuildTarget target, const std::filesystem::path& self) {
  if (target == host_build_target()) return self;
  if (target == BuildTarget::Android) return std::nullopt;
  std::filesystem::path runtime = self.parent_path() / kRuntimesFolder / build_target_name(target) /
                                  platform::executable_file_name(kRuntimeName, target == BuildTarget::Windows);
  std::error_code fs_error;
  if (!std::filesystem::is_regular_file(runtime, fs_error)) return std::nullopt;
  return runtime;
}

std::optional<std::filesystem::path> build_game_for(const std::filesystem::path& game_dir,
                                                    const std::filesystem::path& out_dir, BuildTarget target,
                                                    std::string& error) {
  if (target == BuildTarget::Android) {
    const std::filesystem::path runtimes = platform::executable_path().parent_path() / kRuntimesFolder / kAndroidTarget;
    return build_android_game(game_dir, out_dir, runtimes, error);
  }
  const auto runtime = runtime_for(target, platform::executable_path());
  if (!runtime) {
    error = std::string("the ") + build_target_name(target) + " runtime is not built yet: run tools/" +
            build_target_name(target) + "/build_runtime.sh";
    return std::nullopt;
  }
  if (!build_game(game_dir, out_dir, *runtime, target, error)) return std::nullopt;
  return normalized_folder(out_dir) /
         platform::executable_file_name(executable_name(game_dir), target == BuildTarget::Windows);
}

namespace {

// The engine as the game's executable, allowed to run.
bool copy_engine(const std::filesystem::path& engine_binary, const std::filesystem::path& binary, std::string& error) {
  std::error_code fs_error;
  std::filesystem::copy_file(engine_binary, binary, std::filesystem::copy_options::overwrite_existing, fs_error);
  if (fs_error) {
    error = "cannot copy the engine: " + fs_error.message();
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

// What is built is a game, and not into itself.
bool can_build(const std::filesystem::path& game_dir, const std::filesystem::path& out_dir, std::string& error) {
  if (!std::filesystem::exists(game_dir / config::kSceneFileName)) {
    error = game_dir.string() + " is not a game (no " + config::kSceneFileName + ")";
    return false;
  }
  if (!folder_inside(out_dir, game_dir)) return true;
  error = "cannot build into " + out_dir.string() + ": it is inside the game folder";
  return false;
}

}  // namespace

bool build_game(const std::filesystem::path& game_folder, const std::filesystem::path& out_folder,
                const std::filesystem::path& engine_binary, BuildTarget target, std::string& error) {
  const std::filesystem::path game_dir = normalized_folder(game_folder);
  const std::filesystem::path out_dir = normalized_folder(out_folder);
  if (!can_build(game_dir, out_dir, error)) return false;
  std::error_code fs_error;
  std::filesystem::create_directories(out_dir, fs_error);
  const std::filesystem::path binary =
      out_dir / platform::executable_file_name(executable_name(game_dir), target == BuildTarget::Windows);
  return copy_engine(engine_binary, binary, error) && copy_game(game_dir, out_dir / kBundledGameFolder, error) &&
         write_notices(out_dir, error);
}

std::vector<std::filesystem::path> shipped_files(const std::filesystem::path& game_dir) {
  std::vector<std::filesystem::path> files;
  std::error_code fs_error;
  std::error_code unreadable;  // (one entry that cannot be looked at does not end the walk)
  std::filesystem::recursive_directory_iterator walk(game_dir, fs_error);
  for (; !fs_error && walk != std::filesystem::end(walk); walk.increment(fs_error)) {
    if (!ships(*walk)) {
      walk.disable_recursion_pending();  // nothing inside a folder that stays behind is looked at
    } else if (walk->is_regular_file(unreadable)) {
      files.push_back(walk->path().lexically_relative(game_dir));
    }
  }
  std::sort(files.begin(), files.end());
  return files;
}

bool signing_key_file(const std::filesystem::path& file) {
  const std::string extension = file.extension().string();
  return std::any_of(std::begin(kSigningKeyExtensions), std::end(kSigningKeyExtensions),
                     [&extension](const char* known) { return extension == known; });
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
  // It sits next to the game's files: it cannot have their folder's name.
  if (name.empty() || name == kBundledGameFolder) return kFallbackExecutable;
  return name;
}

}  // namespace redgem
