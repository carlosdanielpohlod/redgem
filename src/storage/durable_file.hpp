#pragma once

#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

// Writing a file so that what was written is on the disk, not only in the system's buffers, when
// the writing returns: a power cut right after a save keeps it.
namespace redgem::durable {

// Pushes what was written to an open file out to the disk; false when it could not.
using Flush = std::function<bool(std::FILE* file)>;

// The flush of the system this runs on (fsync; _commit on Windows; on the Mac, F_FULLFSYNC, which
// asks the drive to empty its own cache too).
bool flush_to_disk(std::FILE* file);

// Writes `text` into `file` (replacing what it held) and flushes it with `flush`. False when not
// all of it reached the disk. It opens `file` as it is, through a link too: only for a name nothing
// else can have chosen (a file in a folder just made for it). replace_file_via is for any other.
bool write_flushed(const std::filesystem::path& file, const std::string& text, const Flush& flush = flush_to_disk);

// Pushes a folder's list of files to the disk, so that a file just renamed into it stays renamed
// after a power cut (fsync of the folder). Windows has no such flush: there it does nothing.
// False when it could not.
bool sync_folder(const std::filesystem::path& folder);

// Replaces what `file` holds with `text`, whole: written beside it as <file>.<random>.part,
// flushed, moved over it and the move pushed to the disk. False when `file` could not be replaced:
// it holds what it held, and nothing this write made is left beside it.
bool replace_file(const std::filesystem::path& file, std::string_view text, const Flush& flush = flush_to_disk);

// replace_file, its partial file named <file>.<random><partial_suffix>. That file is this write's
// own: made new under a name no other write has (two writes of one file never share one), never
// opened through a link, and the one moved over `file` whatever has its name by then (see
// staged_write.hpp). Nothing already in the folder is opened, written or removed.
bool replace_file_via(const std::filesystem::path& file, const char* partial_suffix, std::string_view text,
                      const Flush& flush = flush_to_disk);

// replace_file_via without the trips to the disk: the file is never left half written, but a power
// cut right after may lose it (a scene save, an agent's edit: the editor's work, not a player's).
// False, with why in `error`, when `file` could not be replaced.
bool replace_file_quickly(const std::filesystem::path& file, const char* partial_suffix, std::string_view text,
                          std::string& error);

}  // namespace redgem::durable
