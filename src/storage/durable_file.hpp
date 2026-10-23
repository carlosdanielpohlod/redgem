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

}  // namespace redgem::durable
