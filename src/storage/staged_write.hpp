#pragma once

// How durable::replace_file_via writes: each write in a file of its own, which only that write can
// move or remove, moved over the file it replaces. (Inside the storage code; see durable_file.hpp.)

#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

#include "storage/durable_file.hpp"

namespace redgem::durable::staging {

// Writes `text` into a file that is this write's own, beside `file` and named
// <file>.<random><suffix>, flushes it with `flush` and moves it over `file`. Nothing already in the
// folder is opened, written or removed, and what is moved is the file written, whatever has its
// name by then: on Linux, the Mac and Android it is made inside a folder of the write's own
// (closed to other users) and moved from that folder's descriptor; on Windows it is held open,
// shared with nobody, and moved by its handle. Why it could not; empty when it could.
std::string write_staged(const std::filesystem::path& file, const char* suffix, std::string_view text,
                         const Flush& flush);

// What both systems' writes share (staged_write.cpp).

// A name for one write's own file or folder: <file's name>.<random><suffix>.
std::filesystem::path staged_name(const std::filesystem::path& file, const char* suffix);
// The folder a file is in ("." for a name with no folder).
std::filesystem::path folder_of(const std::filesystem::path& file);
// Writes all of `text` into the open file, flushes it with `flush` and closes it. False when not
// all of it reached where `flush` pushes it.
bool write_and_close(std::FILE* out, std::string_view text, const Flush& flush);
// Why a write failed: what the system said, else that not all of it was written.
std::error_code write_error();

// Names tried for a write's own file before giving up. Each is random: a name already taken is a
// sign of something else at work in the folder, never of bad luck.
inline constexpr int kMostNameTries = 8;

}  // namespace redgem::durable::staging
