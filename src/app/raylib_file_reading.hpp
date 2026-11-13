#pragma once

// Raylib's own loaders (sounds, and whatever else raylib opens by a path) reading through the
// engine's reading of a game's files (core/game_files.hpp), so a built game's packed files are
// found in its archive. Files outside the archive are read from the disk, as raylib would.
namespace redgem {

// Hands raylib the engine's reading (SetLoadFileDataCallback, SetLoadFileTextCallback). Once a
// game archive is mounted; harmless to call again.
void read_raylib_files_through_game_files();

}  // namespace redgem
