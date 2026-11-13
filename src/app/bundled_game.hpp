#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace redgem {

// The folder a built game's files are in, next to its executable. A build packs them into one
// archive instead (kArchiveFileName, game.rgpak, beside where that folder would be: see
// core/game_archive.hpp), unless it was asked to keep them loose (redgem build --loose).
inline constexpr const char* kBundledGameFolder = "game";
// A game built for the Mac is an app: <name>.app/Contents/MacOS/<executable> plays the game in
// <name>.app/Contents/Resources/game (or game.rgpak).
inline constexpr const char* kMacContentsFolder = "Contents";
inline constexpr const char* kMacProgramFolder = "MacOS";
inline constexpr const char* kMacResourcesFolder = "Resources";
// The game a built executable ships with, when there is one: the game folder next to it (or for
// the program of a Mac app, in <name>.app/Contents/MacOS, the one in the app's resources), packed
// into an archive beside that place or loose in it. Nothing is opened: this only looks.
std::optional<std::filesystem::path> bundled_game(const std::filesystem::path& executable);

// Opens the game a built executable ships with, to be played: a packed game's archive is checked
// and mounted as its game folder (see core/game_files.hpp), and raylib's loaders read from it. The
// game folder; nullopt with `error` saying why when there is no game, or its archive is damaged.
std::optional<std::filesystem::path> open_bundled_game(const std::filesystem::path& executable, std::string& error);
// The same for a game kept in `place` (its game.rgpak or its game/ folder there): Android's player
// unpacks the game into its storage and opens it from there.
std::optional<std::filesystem::path> open_game_in(const std::filesystem::path& place, std::string& error);

}  // namespace redgem
