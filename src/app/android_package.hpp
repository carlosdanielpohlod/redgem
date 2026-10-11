#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>

namespace redgem {

// Packing a game as an Android app (.apk): the game's files as the app's assets, the engine built
// for each kind of processor as its native library, signed so a phone accepts it. The packing
// itself is done by the Android SDK's own tools and Java's keytool, which must be installed.

// What is inside the APK besides the game.
inline constexpr const char* kAndroidManifestAsset = "game.files";   // the game's files, one path per line
inline constexpr const char* kAndroidStampAsset = "game.stamp";      // changes with every build
inline constexpr const char* kAndroidLibraryName = "redgem";         // lib/<abi>/libredgem.so

struct AndroidTools {
  std::filesystem::path aapt;        // adds files to an APK
  std::filesystem::path aapt2;       // makes the APK from the manifest and assets
  std::filesystem::path zipalign;
  std::filesystem::path apksigner;
  std::filesystem::path platform;    // android.jar of an installed platform
};

// The SDK's tools, from ANDROID_HOME or ANDROID_SDK_ROOT (the newest build-tools and platform
// installed). nullopt, with `error` saying what is missing, when they are not all there.
std::optional<AndroidTools> find_android_tools(const std::filesystem::path& sdk, std::string& error);
// The SDK folder the environment points at; empty when it points nowhere.
std::filesystem::path android_sdk_from_environment();

// The app's id: "com.redgem." and the game's folder name. A name of small letters and digits is
// used as it is ("platformer" -> "com.redgem.platformer"); anything else is dropped and comes
// back as a few digits, so "My Game" and "my-game" stay two apps ("com.redgem.mygame_3fa2c1").
// A name with no letter to start with gets "game" before it.
std::string android_package_name(const std::filesystem::path& game_dir);
// The app's AndroidManifest.xml: a full-screen, landscape native activity running the engine.
std::string android_manifest(const std::string& package, const std::string& label);

// The engines built for Android, by kind of processor ("arm64-v8a" -> .../libredgem.so), found
// in `runtimes_dir`/<abi>/. Empty when none is built.
std::map<std::string, std::filesystem::path> android_runtimes(const std::filesystem::path& runtimes_dir);

// Lays out in `stage_dir` what goes into the APK: assets/game/... (a copy of the game, without
// hidden files and editors' backups),
// assets/game.files, assets/game.stamp, lib/<abi>/libredgem.so and AndroidManifest.xml.
bool stage_android_game(const std::filesystem::path& game_dir,
                        const std::map<std::string, std::filesystem::path>& runtimes, const std::string& stamp,
                        const std::filesystem::path& stage_dir, std::string& error);

// Makes `apk` out of a staged folder with the SDK's tools, signed with `keystore` (created on
// first use: a key good for installing on one's own devices, not for a store).
bool pack_android_apk(const AndroidTools& tools, const std::filesystem::path& stage_dir,
                      const std::filesystem::path& keystore, const std::filesystem::path& apk, std::string& error);

// The whole thing: <out_dir>/<game>.apk from a game folder. Returns the APK, or nullopt with `error` set.
std::optional<std::filesystem::path> build_android_game(const std::filesystem::path& game_dir,
                                                        const std::filesystem::path& out_dir,
                                                        const std::filesystem::path& runtimes_dir, std::string& error);

}  // namespace redgem
