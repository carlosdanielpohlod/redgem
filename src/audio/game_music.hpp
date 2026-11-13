#pragma once

#include <raylib.h>

#include <filesystem>

// Music streams of a game's sound files. Raylib streams music by opening the file itself, by its
// path, which a built game's archive has no file for (see core/game_files.hpp): a file of the
// archive is read whole and streamed from memory instead, and that memory is kept until the
// stream is unloaded. A file on the disk is streamed from the disk, as before.
namespace redgem {

// A stream of the file; raylib's invalid Music when it cannot be loaded.
Music load_game_music(const std::filesystem::path& file);
// Unloads a stream from load_game_music, and the memory it was streamed from.
void unload_game_music(Music music);

}  // namespace redgem
