#include "storage/durable_file.hpp"

#include <cerrno>
#include <system_error>

#include "core/log.hpp"

#ifdef _WIN32
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace redgem::durable {
namespace {

constexpr const char* kPartialSuffix = ".part";  // of a file still being written
constexpr const char* kThisFolder = ".";         // where a file named with no folder is

#ifdef _WIN32
// Bytes as they are: a save written on Windows reads the same everywhere.
constexpr const wchar_t* kWriteBytes = L"wb";
#else
constexpr const char* kWriteBytes = "wb";
#endif

std::FILE* open_for_writing(const std::filesystem::path& file) {
#ifdef _WIN32
  return _wfopen(file.c_str(), kWriteBytes);  // (a wide name: any user's folder opens)
#else
  return std::fopen(file.c_str(), kWriteBytes);
#endif
}

#ifdef __APPLE__
// Whether F_FULLFSYNC failed only because the file system does not take it (a network or FAT
// drive): those get the plain fsync. Any other failure is the disk's, and is one.
bool full_sync_not_taken(int error) { return error == ENOTSUP || error == EINVAL; }

int full_sync(int descriptor) {
  if (fcntl(descriptor, F_FULLFSYNC) == 0) return 0;
  return full_sync_not_taken(errno) ? fsync(descriptor) : -1;
}
#endif

// Zero when what the system holds of the file is on the disk.
int sync_to_disk(std::FILE* file) {
#if defined(_WIN32)
  return _commit(_fileno(file));
#elif defined(__APPLE__)
  return full_sync(fileno(file));
#else
  return fsync(fileno(file));
#endif
}

// Writes all of `text` and flushes it.
bool write_and_flush(std::FILE* out, const std::string& text, const Flush& flush) {
  if (std::fwrite(text.data(), 1, text.size(), out) != text.size()) return false;
  return flush(out);
}

std::filesystem::path folder_of(const std::filesystem::path& file) {
  return file.has_parent_path() ? file.parent_path() : std::filesystem::path(kThisFolder);
}

// Writes `partial` flushed, then moves it over `file`.
bool write_and_move(const std::filesystem::path& partial, const std::filesystem::path& file, const std::string& text,
                    const Flush& flush) {
  if (!write_flushed(partial, text, flush)) return false;
  std::error_code fs_error;
  std::filesystem::rename(partial, file, fs_error);
  return !fs_error;
}

}  // namespace

bool flush_to_disk(std::FILE* file) {
  if (std::fflush(file) != 0) return false;
  return sync_to_disk(file) == 0;
}

bool write_flushed(const std::filesystem::path& file, const std::string& text, const Flush& flush) {
  std::FILE* out = open_for_writing(file);
  if (out == nullptr) return false;
  const bool flushed = write_and_flush(out, text, flush);
  // A full disk can show only here: the file being replaced must not be lost to half a one.
  if (std::fclose(out) != 0) return false;
  return flushed;
}

bool sync_folder(const std::filesystem::path& folder) {
#ifdef _WIN32
  (void)folder;
  return true;
#else
  const int descriptor = open(folder.c_str(), O_RDONLY);
  if (descriptor < 0) return false;
  const bool synced = fsync(descriptor) == 0;
  close(descriptor);
  return synced;
#endif
}

bool replace_file(const std::filesystem::path& file, const std::string& text, const Flush& flush) {
  const std::filesystem::path partial = file.string() + kPartialSuffix;
  if (!write_and_move(partial, file, text, flush)) {
    std::error_code fs_error;
    std::filesystem::remove(partial, fs_error);
    return false;
  }
  // (The file is replaced either way: only a power cut in the next moments could undo it.)
  if (!sync_folder(folder_of(file))) log::warning("the folder of " + file.string() + " could not be flushed to disk");
  return true;
}

}  // namespace redgem::durable
