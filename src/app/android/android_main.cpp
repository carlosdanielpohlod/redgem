// The player's entry point on Android (the library a game's app loads, made of the runtime alone). The game's files
// travel inside the APK; on the first run (and after every update) they are copied to the app's own storage, and the
// game plays from there.

#include <android/asset_manager.h>
#include <android/configuration.h>
#include <android_native_app_glue.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "app/android/android_bundle.hpp"
#include "app/user_settings.hpp"
#include "app/window_presenter.hpp"
#include "app/bundled_game.hpp"
#include "app/game_texts.hpp"
#include "core/exit_codes.hpp"
#include "core/log.hpp"
#include "core/platform.hpp"
#include "core/translations.hpp"
#include "input/raylib_input_source.hpp"
#include "input/touch_input_source.hpp"

// The running activity, kept by raylib's Android layer.
extern "C" struct android_app* GetAndroidApp(void);

namespace {

using namespace redgem;

// Where saves and settings are kept inside the app's own storage (where they have always been:
// moving them would lose the saves of the games already installed).
constexpr const char* kUserDataFolder = ".config/redgem";
constexpr std::size_t kCopyChunkBytes = 64 * 1024;
constexpr std::size_t kLanguageLetters = 2;  // of the language and the country the phone is set to ("pt", "BR")

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
  std::istringstream manifest(read_asset(assets, kAndroidManifestAsset));
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

// Whether the game of this very build is in the app's storage already: its stamp is the one kept there.
bool already_unpacked(const std::string& stamp, const std::filesystem::path& stamp_file) {
  return !stamp.empty() && read_file(stamp_file) == stamp;
}

// Copies the game out of the APK unless this build of it is already there.
bool unpack_game(AAssetManager* assets, const std::filesystem::path& home) {
  const std::string stamp = read_asset(assets, kAndroidStampAsset);
  const std::filesystem::path stamp_file = home / kAndroidStampAsset;
  if (already_unpacked(stamp, stamp_file)) return true;
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
  // (A screen that does not say how large it is has no shape to follow.)
  const bool measured = one_side > 0.0f && other_side > 0.0f;
  return measured ? std::max(one_side, other_side) / std::min(one_side, other_side) : 0.0f;
}

// Two letters the configuration gives (a language, a country); empty when it gives none.
template <typename Read>
std::string two_letters(const android_app* app, Read read) {
  char letters[kLanguageLetters] = {};
  read(app->config, letters);
  return std::string(letters, strnlen(letters, kLanguageLetters));
}

// The language the phone is set to, with its country ("pt_BR"); empty when it says none.
std::string phone_language(const android_app* app) {
  const std::string language = two_letters(app, AConfiguration_getLanguage);
  const std::string country = two_letters(app, AConfiguration_getCountry);
  return platform::language_of_locale(country.empty() ? language : language + "_" + country);
}

// Plays the game that was unpacked into the app's storage, on the whole screen, with on-screen
// controls for whoever has no gamepad.
int play_unpacked_game(const android_app* app) {
  GameWindowOptions options;
  options.game_dir = kBundledGameFolder;
  options.screen_aspect = screen_aspect(app);
  const PlayerDisplay display = player_display(options.game_dir, user_settings_file());
  options.display = display.display;
  options.display_kept_in = display.kept_in;
  const RaylibInputSource devices;
  TouchInputSource touch_screen(devices);
  return play_in_window(options, touch_screen);
}

}  // namespace

int main(int, char**) {
  const android_app* app = GetAndroidApp();
  const std::filesystem::path home = app->activity->internalDataPath;
  // Saves and settings live in the app's storage: the platform layer is told where.
  platform::set_user_data_dir(home / kUserDataFolder);
  // The game is played from a path relative to the app's storage ("game"), with that storage as
  // the working folder. It cannot be an absolute path: raylib opens every file it loads on Android
  // (pictures, sounds, music) through its own fopen, which first looks in the APK's assets and
  // then prepends the app's storage path to whatever name it was given, absolute or not. The
  // engine's own reading (the scene, the scripts, game.json) goes through the C++ library, which
  // resolves a relative path against the working folder. Changing to the storage folder is what
  // makes both mean the same file.
  const bool unpacked = chdir(home.c_str()) == 0 && unpack_game(app->activity->assetManager, home);
  if (!unpacked) {
    log::error("the game could not be unpacked into " + home.string());
    return kExitFailure;
  }
  try {
    add_game_texts();
    translations::use_language(phone_language(app));
    return play_unpacked_game(app);
  } catch (const std::exception& error) {
    log::error(error.what());
    return kExitFailure;
  }
}
