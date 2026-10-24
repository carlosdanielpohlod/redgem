#include "project/android/android_package.hpp"

#include <chrono>

#include "core/files.hpp"
#include "core/platform.hpp"
#include "project/android/android_package_names.hpp"
#include "project/game_build.hpp"

namespace redgem {

namespace {

constexpr const char* kApkExtension = ".apk";
constexpr const char* kBundleExtension = ".aab";
constexpr const char* kPathVariable = "PATH";
constexpr const char* kStageFolder = ".redgem-apk-stage";  // inside the output folder, gone when the build ends

// How the staged game is packed: with the SDK's tools, signed with the key, as an APK or (with
// bundletool) as an App Bundle.
struct Packer {
  AndroidTools tools;
  SigningKey key;
  AndroidFormat format = AndroidFormat::Apk;
  std::string bundletool;  // how bundletool is run; empty for an APK
};

bool pack(const Packer& packer, const std::filesystem::path& stage, const std::filesystem::path& out,
          std::string& error) {
  if (packer.format == AndroidFormat::Apk) return pack_android_apk(packer.tools, stage, packer.key, out, error);
  return pack_android_bundle(packer.tools, packer.bundletool, stage, packer.key, out, error);
}

// Stages and packs the game in a folder of its own beside the app, gone when the build ends.
bool build_app(const std::filesystem::path& game_dir, const std::map<std::string, std::filesystem::path>& runtimes,
               const Packer& packer, const std::filesystem::path& out, std::string& error) {
  const std::filesystem::path stage = out.parent_path() / kStageFolder;
  // (The stamp only has to differ from the last build's.)
  const std::string stamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
  const bool built = stage_android_game(game_dir, runtimes, stamp, stage, error) && pack(packer, stage, out, error);
  std::error_code fs_error;
  std::filesystem::remove_all(stage, fs_error);  // whether it worked or not
  return built;
}

// How bundletool is run for `format` (nothing for an APK); nullopt, with `error` set, when a
// bundle is asked for and bundletool is nowhere.
std::optional<std::string> bundletool_for(AndroidFormat format, std::string& error) {
  if (format == AndroidFormat::Apk) return std::string();
  return find_bundletool(platform::environment(kBundletoolVariable), platform::environment(kPathVariable), error);
}

// The key the game's settings ask for (see signing_key_for).
std::optional<SigningKey> signing_key_of(const std::filesystem::path& game_dir, std::string& error) {
  AndroidSettings settings;
  if (!read_android_settings(normalized_folder(game_dir), settings, error)) return std::nullopt;
  return signing_key_for(normalized_folder(game_dir), settings,
                         platform::user_data_dir() / android_names::kKeystoreFile, error);
}

// The players a build packs; none, with the folder they were looked for in said, when it is empty.
std::map<std::string, std::filesystem::path> runtimes_to_pack(const std::filesystem::path& runtimes_dir,
                                                              std::string& error) {
  auto runtimes = android_runtimes(runtimes_dir);
  if (runtimes.empty()) error = missing_runtime_message(BuildTarget::Android, any_android_runtime_in(runtimes_dir));
  return runtimes;
}

// The file a game's app is written to in `out_dir`, named after the game.
std::filesystem::path app_in(const std::filesystem::path& game_dir, const std::filesystem::path& out_dir,
                             AndroidFormat format) {
  std::error_code fs_error;
  const std::filesystem::path out = std::filesystem::absolute(normalized_folder(out_dir), fs_error);
  return out / (executable_name(game_dir) + (format == AndroidFormat::Apk ? kApkExtension : kBundleExtension));
}

// Whether a build may start: on a system that has the tools, and not into the game itself.
bool can_build(const std::filesystem::path& game_dir, const std::filesystem::path& out_dir, std::string& error) {
  // (The SDK's tools are run through a POSIX shell.)
  if (platform::has_posix_shell()) return builds_outside_game(game_dir, out_dir, error);
  error = "Android games are built on Linux";
  return false;
}

}  // namespace

bool read_android_settings(const std::filesystem::path& game_dir, AndroidSettings& settings, std::string& error) {
  const GameManifest manifest = read_game_manifest(game_dir);
  settings = manifest.settings.android;
  if (!manifest.android_error.empty()) error = manifest.android_error;
  return manifest.android_error.empty();
}

std::optional<std::filesystem::path> build_android_game(const std::filesystem::path& game_dir,
                                                        const std::filesystem::path& out_dir,
                                                        const std::filesystem::path& runtimes_dir, std::string& error,
                                                        AndroidFormat format) {
  if (!can_build(game_dir, out_dir, error)) return std::nullopt;
  const auto runtimes = runtimes_to_pack(runtimes_dir, error);
  if (runtimes.empty()) return std::nullopt;
  const auto tools = find_android_tools(android_sdk_from_environment(), error);
  const auto bundletool = tools ? bundletool_for(format, error) : std::nullopt;
  const auto key = bundletool ? signing_key_of(game_dir, error) : std::nullopt;
  if (!key) return std::nullopt;
  std::filesystem::path app = app_in(game_dir, out_dir, format);
  if (!build_app(game_dir, runtimes, Packer{*tools, *key, format, *bundletool}, app, error)) return std::nullopt;
  return app;
}

}  // namespace redgem
