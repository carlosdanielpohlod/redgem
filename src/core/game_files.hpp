#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "core/game_archive.hpp"

// The one way the engine reads a game's own files (scripts, scenes, pictures, sounds, fonts,
// texts, game.json). A built game's files are packed into an archive (see game_archive.hpp),
// mounted as the folder the game would have been in: a path inside that folder is read from the
// archive, and from the archive only (a loose file put beside it is never read instead). Any other
// path, and every path while nothing is mounted (a game played from its project folder, the
// editor), is read from the disk. The archive is read-only: saves, settings and the players' data
// folder are written on the disk, elsewhere.
namespace redgem::game_files {

// Mounts `archive` as the folder `root` (replacing what was mounted).
void mount(const std::filesystem::path& root, GameArchive archive);
void unmount();
// The folder the archive is mounted as; nullopt when none is.
std::optional<std::filesystem::path> mounted_root();

// Whether `path` is inside the mounted archive's folder (whether there is such a file or not).
bool in_archive(const std::filesystem::path& path);
// Whether `path` is a file: one of the archive, or a regular file on the disk.
bool is_file(const std::filesystem::path& path);
// Whether `path` is a folder: one some file of the archive is in, or a folder on the disk.
bool is_folder(const std::filesystem::path& path);
// A file's size; nullopt when it is no file.
std::optional<std::uintmax_t> file_size(const std::filesystem::path& path);
// Everything a file holds when it is at most `max_bytes`; nullopt for a bigger one, for a damaged
// one (said in the log), for what is not a regular file (a device or a pipe never ends) and for
// one that cannot be read.
std::optional<std::vector<std::uint8_t>> read(const std::filesystem::path& path, std::uintmax_t max_bytes);
// What is right inside a folder: in the archive its files, on the disk every entry (links and
// subfolders too, for the caller to refuse by name).
std::vector<std::filesystem::path> entries_in(const std::filesystem::path& folder);

}  // namespace redgem::game_files
