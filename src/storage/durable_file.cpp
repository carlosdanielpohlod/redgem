#include "storage/durable_file.hpp"

#include <cerrno>

#include "core/log.hpp"
#include "storage/staged_write.hpp"

#ifdef _WIN32
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace redgem::durable {
namespace {

constexpr const char* kPartialSuffix = ".part";  // of a file still being written

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

// Pushes what was written out of the program's buffers to the system only: a write that may be lost
// to a power cut, though never left half done.
bool flush_to_system(std::FILE* file) { return std::fflush(file) == 0; }

}  // namespace

bool flush_to_disk(std::FILE* file) {
  if (std::fflush(file) != 0) return false;
  return sync_to_disk(file) == 0;
}

bool write_flushed(const std::filesystem::path& file, const std::string& text, const Flush& flush) {
  std::FILE* out = open_for_writing(file);
  if (out == nullptr) return false;
  return staging::write_and_close(out, text, flush);
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

bool replace_file(const std::filesystem::path& file, std::string_view text, const Flush& flush) {
  return replace_file_via(file, kPartialSuffix, text, flush);
}

bool replace_file_via(const std::filesystem::path& file, const char* partial_suffix, std::string_view text,
                      const Flush& flush) {
  if (!staging::write_staged(file, partial_suffix, text, flush).empty()) return false;
  // (The file is replaced either way: only a power cut in the next moments could undo it.)
  const std::filesystem::path folder = staging::folder_of(file);
  if (!sync_folder(folder)) log::warning("the folder of " + file.string() + " could not be flushed to disk");
  return true;
}

bool replace_file_quickly(const std::filesystem::path& file, const char* partial_suffix, std::string_view text,
                          std::string& error) {
  error = staging::write_staged(file, partial_suffix, text, flush_to_system);
  return error.empty();
}

}  // namespace redgem::durable
