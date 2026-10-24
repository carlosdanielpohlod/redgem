// Packing an Android app: laying out in a folder what goes into the APK.

#include "project/android/android_package.hpp"

#include <algorithm>
#include <vector>

#include "app/bundled_game.hpp"
#include "core/files.hpp"
#include "project/android/android_package_names.hpp"
#include "project/game_build.hpp"
#include "project/game_shipping.hpp"
#include "project/notices.hpp"

namespace redgem {

namespace {

constexpr const char* kAnyAbi = "<abi>";     // stands for a kind of processor in a path shown to people
constexpr const char* kLineBreaks = "\n\r";  // what the list of an app's files is split at
constexpr char kSkippedFolderPrefix = '_';   // the SDK's packer leaves such folders out

bool skipped_by_packer(const std::filesystem::path& folder) {
  return !folder.empty() && folder.string().front() == kSkippedFolderPrefix;
}

// Whether a file sits in a folder the SDK's packer silently leaves out of an APK.
bool in_skipped_folder(const std::filesystem::path& relative) {
  const std::filesystem::path folders = relative.parent_path();
  return std::any_of(folders.begin(), folders.end(), skipped_by_packer);
}

bool copy_over(const std::filesystem::path& from, const std::filesystem::path& to, std::error_code& fs_error) {
  std::filesystem::create_directories(to.parent_path(), fs_error);
  std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing, fs_error);
  return !fs_error;
}

// The app finds its files by a list with one path a line: a name with a line break in it would
// read as two other files. False, with `error` set, for such a name.
bool can_be_listed(const std::filesystem::path& relative, std::string& error) {
  if (relative.generic_string().find_first_of(kLineBreaks) == std::string::npos) return true;
  error = "a file in '" + relative.parent_path().generic_string() +
          "' has a line break in its name and cannot go into an Android app. Rename the file.";
  return false;
}

// Copies one of the game's files into the app's assets. False, with `error` set, when it cannot go.
bool stage_file(const std::filesystem::path& file, const std::filesystem::path& relative,
                const std::filesystem::path& assets, std::string& error) {
  if (!can_be_listed(relative, error)) return false;
  if (in_skipped_folder(relative)) {
    error = relative.generic_string() +
            " cannot go into an Android app: folders starting with '_' are left out of "
            "it. Rename the folder.";
    return false;
  }
  std::error_code fs_error;
  if (copy_over(file, assets / kBundledGameFolder / relative, fs_error)) return true;
  error = "cannot copy " + relative.generic_string() + ": " + fs_error.message();
  return false;
}

// Copies the game into the app's assets. `files` gets where each file is inside them: the app
// cannot list what is inside its own APK, so the list of the game's files travels with it.
bool stage_files(const std::filesystem::path& game_dir, const std::filesystem::path& assets,
                 std::vector<std::string>& files, std::string& error) {
  // (The same files as any other build of the game: no links, hidden files, backups or keys.)
  for (const std::filesystem::path& relative : shipped_files(game_dir)) {
    if (!stage_file(game_dir / relative, relative, assets, error)) return false;
    files.push_back((std::filesystem::path(kBundledGameFolder) / relative).generic_string());
  }
  return true;
}

// The paths of the game's files, sorted, one per line.
std::string listing_of(std::vector<std::string> files) {
  std::sort(files.begin(), files.end());
  std::string listing;
  for (const std::string& file : files) listing += file + "\n";
  return listing;
}

bool stage_runtimes(const std::map<std::string, std::filesystem::path>& runtimes,
                    const std::filesystem::path& stage_dir, std::string& error) {
  for (const auto& [abi, library] : runtimes) {
    std::error_code fs_error;
    if (copy_over(library, stage_dir / android_names::kLibrariesFolder / abi / android_library_file(), fs_error))
      continue;
    error = "cannot copy the player for " + abi + ": " + fs_error.message();
    return false;
  }
  return true;
}

// The launcher icon, for a game that has an icon.
bool stage_icon_of(const BuildIdentity& identity, const std::filesystem::path& stage_dir, std::string& error) {
  return identity.icon.empty() || stage_android_icon(identity.icon, stage_dir, error);
}

// The app's manifest, from what the game says about its app (and about itself), and its icon.
bool manifest_for(const std::filesystem::path& game_dir, const std::filesystem::path& stage_dir, std::string& manifest,
                  std::string& error) {
  const GameManifest game = read_game_manifest(game_dir);
  // (An id that cannot be used is no id to name the app by: the build says so instead of guessing.)
  error = game.android_error.empty() ? game.id_error : game.android_error;
  if (!error.empty()) return false;
  const auto identity = build_identity(game_dir, game, error);
  if (!identity || !stage_icon_of(*identity, stage_dir, error)) return false;
  const AndroidAppLook look{identity->identity.version, app_goes_online(identity->identity, game_dir),
                            !identity->icon.empty()};
  manifest =
      android_manifest(android_package_for(game.settings, game_dir), game.settings.title, game.settings.android, look);
  return true;
}

// The folder the app's assets go in, inside a stage folder emptied of what an earlier build left.
std::filesystem::path fresh_assets_folder(const std::filesystem::path& stage_dir) {
  std::error_code fs_error;
  std::filesystem::remove_all(stage_dir, fs_error);
  std::filesystem::path assets = stage_dir / android_names::kAssetsFolder;
  std::filesystem::create_directories(assets, fs_error);
  return assets;
}

// What is checked before anything is staged.
bool can_stage(const std::filesystem::path& game_dir, const std::map<std::string, std::filesystem::path>& runtimes,
               std::string& error) {
  if (!is_game_to_build(game_dir, error)) return false;
  if (runtimes.empty()) error = missing_runtime_message(BuildTarget::Android, any_android_runtime_in({}));
  return !runtimes.empty();
}

// Writes what the app is told about the game it carries, and its manifest.
bool write_listing(const std::filesystem::path& stage_dir, std::vector<std::string> files, const std::string& stamp,
                   const std::string& manifest, std::string& error) {
  const std::filesystem::path assets = stage_dir / android_names::kAssetsFolder;
  const bool written = write_text_file(assets / kAndroidManifestAsset, listing_of(std::move(files)), error) &&
                       write_text_file(assets / kAndroidStampAsset, stamp, error) &&
                       write_text_file(stage_dir / android_names::kManifestFile, manifest, error);
  if (!written) error = "cannot write into " + stage_dir.string();
  return written;
}

}  // namespace

std::map<std::string, std::filesystem::path> android_runtimes(const std::filesystem::path& runtimes_dir) {
  std::map<std::string, std::filesystem::path> runtimes;
  std::error_code fs_error;
  for (const auto& entry : std::filesystem::directory_iterator(runtimes_dir, fs_error)) {
    const std::filesystem::path library = entry.path() / android_library_file();
    if (std::filesystem::is_regular_file(library, fs_error)) runtimes[entry.path().filename().string()] = library;
  }
  return runtimes;
}

std::filesystem::path any_android_runtime_in(const std::filesystem::path& runtimes_dir) {
  return runtimes_dir / kAnyAbi / android_library_file();
}

bool stage_android_game(const std::filesystem::path& game_folder,
                        const std::map<std::string, std::filesystem::path>& runtimes, const std::string& stamp,
                        const std::filesystem::path& stage_dir, std::string& error) {
  const std::filesystem::path game_dir = normalized_folder(game_folder);
  if (!can_stage(game_dir, runtimes, error)) return false;
  const std::filesystem::path assets = fresh_assets_folder(stage_dir);
  std::vector<std::string> files;
  std::string manifest;
  // (The licenses of what the app contains go inside it, beside the game.)
  const bool staged = stage_files(game_dir, assets, files, error) && stage_runtimes(runtimes, stage_dir, error) &&
                      manifest_for(game_dir, stage_dir, manifest, error) && write_notices(assets, error);
  return staged && write_listing(stage_dir, std::move(files), stamp, manifest, error);
}

}  // namespace redgem
