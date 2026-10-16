#pragma once

#include <string>

// What an Android app made from a game carries besides the game's files: the names the packer
// (project/android_package) writes and the engine's Android entry point (app/android_main) reads.
// Nothing but names lives here, so the entry point takes them without taking the packer.
namespace redgem {

inline constexpr const char* kAndroidManifestAsset = "game.files";  // the game's files, one path per line
inline constexpr const char* kAndroidStampAsset = "game.stamp";     // changes with every build
// The engine as the app's native library: what the manifest names, and the file Android loads
// for it (lib/<abi>/libredgem.so).
inline constexpr const char* kAndroidLibraryName = "redgem";
inline std::string android_library_file() { return std::string("lib") + kAndroidLibraryName + ".so"; }

}  // namespace redgem
