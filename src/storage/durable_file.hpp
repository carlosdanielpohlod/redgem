#pragma once

#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>

// Writing a file so that what was written is on the disk, not only in the system's buffers, when
// the writing returns: a power cut right after a save keeps it.
namespace redgem::durable {

// Pushes what was written to an open file out to the disk; false when it could not.
using Flush = std::function<bool(std::FILE* file)>;

// The flush of the system this runs on (fsync; _commit on Windows; on the Mac, F_FULLFSYNC, which
// asks the drive to empty its own cache too).
bool flush_to_disk(std::FILE* file);

// Writes `text` into `file` (replacing what it held) and flushes it with `flush`. False when not
// all of it reached the disk.
bool write_flushed(const std::filesystem::path& file, const std::string& text, const Flush& flush = flush_to_disk);

// Pushes a folder's list of files to the disk, so that a file just renamed into it stays renamed
// after a power cut (fsync of the folder). Windows has no such flush: there it does nothing.
// False when it could not.
bool sync_folder(const std::filesystem::path& folder);

// Replaces what `file` holds with `text`, whole: written beside it as <file>.part, flushed, moved
// over it and the move pushed to the disk. False when `file` could not be replaced: it holds what
// it held, and nothing is left beside it.
bool replace_file(const std::filesystem::path& file, const std::string& text, const Flush& flush = flush_to_disk);

}  // namespace redgem::durable
