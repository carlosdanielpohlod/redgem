#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>

#include "app/android_bundle.hpp"
#include "app/game_settings.hpp"

namespace redgem {

// Packing a game as an Android app (.apk): the game's files as the app's assets, the engine built
// for each kind of processor as its native library, signed so a phone accepts it. The packing
// itself is done by the Android SDK's own tools and Java's keytool, which must be installed.

struct AndroidTools {
  std::filesystem::path aapt;   // adds files to an APK
  std::filesystem::path aapt2;  // makes the APK from the manifest and assets
  std::filesystem::path zipalign;
  std::filesystem::path apksigner;
  std::filesystem::path platform;  // android.jar of an installed platform
};

// The SDK's tools, from ANDROID_HOME or ANDROID_SDK_ROOT (the newest build-tools and platform
// installed). nullopt, with `error` saying what is missing, when they are not all there.
std::optional<AndroidTools> find_android_tools(const std::filesystem::path& sdk, std::string& error);
// The SDK folder the environment points at; empty when it points nowhere.
std::filesystem::path android_sdk_from_environment();

// What the game says about its Android app (see AndroidSettings in app/game_settings.hpp); false,
// with `error` set, when something there cannot be used.
bool read_android_settings(const std::filesystem::path& game_dir, AndroidSettings& settings, std::string& error);

// The key an APK is signed with. Its passwords are never written down or put on a command line:
// the signing tool reads them from the environment variables named here.
struct SigningKey {
  std::filesystem::path keystore;
  std::string alias;
  std::string store_password_variable;
  std::string key_password_variable;
};
// The environment variables holding the passwords of a developer's own keystore (the key's own
// password defaults to the keystore's).
inline constexpr const char* kKeystorePasswordVariable = "REDGEM_KEYSTORE_PASSWORD";
inline constexpr const char* kKeyPasswordVariable = "REDGEM_KEY_PASSWORD";
// The key to sign a game with: the developer's, when the game names one (it must exist, and its
// password must be in the environment), else the engine's own in `engine_keystore`, made on first
// use. That one only says "the same machine built this again": fine for one's own devices and
// for sharing the file, not for a store.
std::optional<SigningKey> signing_key_for(const std::filesystem::path& game_dir, const AndroidSettings& settings,
                                          const std::filesystem::path& engine_keystore, std::string& error);

// The app's id: "com.redgem." and the game's folder name. A name of small letters and digits is
// used as it is ("platformer" -> "com.redgem.platformer"); anything else is dropped and comes
// back as a few digits, so "My Game" and "my-game" stay two apps ("com.redgem.mygame_3fa2c1").
// A name with no letter to start with gets "game" before it.
std::string android_package_name(const std::filesystem::path& game_dir);
// The same, made from any name instead of a folder's.
std::string android_package_from(const std::string& name);
// The id of a game's app: the package its game.json gives the app; else the game's own id, as it
// is when it reads as an app's id ("com.mystudio.mygame") and made into one when it does not
// ("my-game" -> "com.redgem.mygame_3fa2c1"); else the one made from its folder's name.
std::string android_package_for(const GameSettings& settings, const std::filesystem::path& game_dir);
// The app's AndroidManifest.xml: a full-screen, landscape native activity running the engine.
std::string android_manifest(const std::string& package, const std::string& label,
                             const AndroidSettings& settings = {});

// The players built for Android (the runtime as a library the system loads, see
// app/android_main.cpp), by kind of processor ("arm64-v8a" -> .../libredgem.so), found in
// `runtimes_dir`/<abi>/. Empty when there is none.
std::map<std::string, std::filesystem::path> android_runtimes(const std::filesystem::path& runtimes_dir);

// Where a player for Android is looked for under `runtimes_dir`, whatever its kind of processor:
// the path shown to whoever has none.
std::filesystem::path any_android_runtime_in(const std::filesystem::path& runtimes_dir);

// Lays out in `stage_dir` what goes into the APK: assets/game/... (a copy of the game, without
// hidden files and editors' backups),
// assets/game.files, assets/game.stamp, lib/<abi>/libredgem.so and AndroidManifest.xml.
bool stage_android_game(const std::filesystem::path& game_dir,
                        const std::map<std::string, std::filesystem::path>& runtimes, const std::string& stamp,
                        const std::filesystem::path& stage_dir, std::string& error);

// Lets only its owner read and write a file: a signing key is nobody else's on a shared machine.
void keep_to_owner(const std::filesystem::path& file);

// Makes `apk` out of a staged folder with the SDK's tools, signed with `key`.
bool pack_android_apk(const AndroidTools& tools, const std::filesystem::path& stage_dir, const SigningKey& key,
                      const std::filesystem::path& apk, std::string& error);

// The whole thing: <out_dir>/<game>.apk from a game folder. Returns the APK, or nullopt with `error` set.
std::optional<std::filesystem::path> build_android_game(const std::filesystem::path& game_dir,
                                                        const std::filesystem::path& out_dir,
                                                        const std::filesystem::path& runtimes_dir, std::string& error);

}  // namespace redgem
