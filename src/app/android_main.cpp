// The engine's entry point on Android. The game's files travel inside the APK; on the first run
// (and after every update) they are copied to the app's own storage, and the game plays from there.

#include <android/asset_manager.h>
#include <android/configuration.h>
#include <android_native_app_glue.h>
#include <unistd.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "app/engine.hpp"
#include "app/game_project.hpp"
#include "core/exit_codes.hpp"
#include "core/log.hpp"
#include "core/platform.hpp"

// The running activity, kept by raylib's Android layer.
extern "C" struct android_app* GetAndroidApp(void);

namespace {

using namespace redgem;

constexpr const char* kHomeVariable = "HOME";
constexpr const char* kManifestAsset = "game.files";  // the paths of the game's files, one per line
constexpr const char* kStampAsset = "game.stamp";     // changes with every build of the APK
constexpr std::size_t kCopyChunkBytes = 64 * 1024;

std::string read_asset(AAssetManager* assets, const std::string& name) {
  AAsset* asset = AAssetManager_open(assets, name.c_str(), AASSET_MODE_BUFFER);
  if (asset == nullptr) return {};
  std::string text(static_cast<std::size_t>(AAsset_getLength(asset)), '\0');
  AAsset_read(asset, text.data(), text.size());
  AAsset_close(asset);
  return text;
}

bool copy_asset(AAssetManager* assets, const std::string& name, const std::filesystem::path& to) {
  AAsset* asset = AAssetManager_open(assets, name.c_str(), AASSET_MODE_STREAMING);
  if (asset == nullptr) return false;
  std::error_code ignored;
  std::filesystem::create_directories(to.parent_path(), ignored);
  std::ofstream out(to, std::ios::binary);
  std::vector<char> chunk(kCopyChunkBytes);
  for (int count = AAsset_read(asset, chunk.data(), chunk.size()); count > 0;
       count = AAsset_read(asset, chunk.data(), chunk.size())) {
    out.write(chunk.data(), count);
  }
  AAsset_close(asset);
  return static_cast<bool>(out);
}

// What a file says; nothing when it is not there.
std::string read_file(const std::filesystem::path& file) {
  std::ifstream in(file);
  std::stringstream text;
  text << in.rdbuf();
  return text.str();
}

// Copies the files the APK's list names into `home`. How many there were; 0, with the reason
// logged, when the game could not be unpacked.
int copy_game_files(AAssetManager* assets, const std::filesystem::path& home) {
  std::istringstream manifest(read_asset(assets, kManifestAsset));
  int copied = 0;
  for (std::string path; std::getline(manifest, path);) {
    if (path.empty()) continue;
    if (!copy_asset(assets, path, home / path)) {
      log::error("cannot unpack " + path);
      return 0;
    }
    ++copied;
  }
  if (copied == 0) log::error("the app carries no game files");
  return copied;
}

// Copies the game out of the APK unless this build of it is already there.
bool unpack_game(AAssetManager* assets, const std::filesystem::path& home) {
  const std::string stamp = read_asset(assets, kStampAsset);
  const std::filesystem::path stamp_file = home / kStampAsset;
  if (!stamp.empty() && read_file(stamp_file) == stamp) return true;
  std::error_code ignored;
  std::filesystem::remove_all(home / kBundledGameFolder, ignored);
  const int copied = copy_game_files(assets, home);
  if (copied == 0) return false;
  // (If the stamp cannot be written the game still runs: it is only unpacked again next time.)
  std::ofstream(stamp_file) << stamp;
  log::info("unpacked " + std::to_string(copied) + " game files");
  return true;
}

// How much wider than tall the screen is, held sideways.
float screen_aspect(const android_app* app) {
  const float one_side = static_cast<float>(AConfiguration_getScreenWidthDp(app->config));
  const float other_side = static_cast<float>(AConfiguration_getScreenHeightDp(app->config));
  if (one_side <= 0.0f || other_side <= 0.0f) return 0.0f;
  return std::max(one_side, other_side) / std::min(one_side, other_side);
}

// Plays the game that was unpacked into the app's storage, on the whole screen.
int play_unpacked_game(const android_app* app) {
  EngineOptions options;
  options.game_dir = kBundledGameFolder;
  options.screen_aspect = screen_aspect(app);
  auto created = Engine::create(options);
  if (auto* error = std::get_if<std::string>(&created)) {
    log::error(*error);
    return kExitFailure;
  }
  return std::get<std::unique_ptr<Engine>>(created)->run();
}

}  // namespace

int main(int, char**) {
  const android_app* app = GetAndroidApp();
  const std::filesystem::path home = app->activity->internalDataPath;
  // Saves and settings live in the app's storage, and the game's files are found from there.
  platform::set_environment(kHomeVariable, home.string(), true);
  if (chdir(home.c_str()) != 0 || !unpack_game(app->activity->assetManager, home)) {
    log::error("the game could not be unpacked into " + home.string());
    return kExitFailure;
  }
  try {
    return play_unpacked_game(app);
  } catch (const std::exception& error) {
    log::error(error.what());
    return kExitFailure;
  }
}
