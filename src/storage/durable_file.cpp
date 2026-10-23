#include "storage/durable_file.hpp"

#ifdef _WIN32
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace redgem::durable {
namespace {

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

// Zero when what the system holds of the file is on the disk.
int sync_to_disk(std::FILE* file) {
#if defined(_WIN32)
  return _commit(_fileno(file));
#elif defined(__APPLE__)
  // (Not every file system takes F_FULLFSYNC: those that do not get the plain fsync.)
  return fcntl(fileno(file), F_FULLFSYNC) == 0 ? 0 : fsync(fileno(file));
#else
  return fsync(fileno(file));
#endif
}

}  // namespace

bool flush_to_disk(std::FILE* file) { return std::fflush(file) == 0 && sync_to_disk(file) == 0; }

bool write_flushed(const std::filesystem::path& file, const std::string& text, const Flush& flush) {
  std::FILE* out = open_for_writing(file);
  if (out == nullptr) return false;
  const bool written = std::fwrite(text.data(), 1, text.size(), out) == text.size();
  const bool flushed = written && flush(out);
  // A full disk can show only here: the file being replaced must not be lost to half a one.
  const bool closed = std::fclose(out) == 0;
  return flushed && closed;
}

}  // namespace redgem::durable
