#pragma once

#include <filesystem>
#include <optional>

namespace redgem {

// The folder a built game's files are in, next to its executable.
inline constexpr const char* kBundledGameFolder = "game";
// A game built for the Mac is an app: <name>.app/Contents/MacOS/<executable> plays the game in
// <name>.app/Contents/Resources/game.
inline constexpr const char* kMacContentsFolder = "Contents";
inline constexpr const char* kMacProgramFolder = "MacOS";
inline constexpr const char* kMacResourcesFolder = "Resources";
// The game a built executable ships with, when there is one: the game folder next to it, or for
// the program of a Mac app (in <name>.app/Contents/MacOS) the one in the app's resources.
std::optional<std::filesystem::path> bundled_game(const std::filesystem::path& executable);

}  // namespace redgem
