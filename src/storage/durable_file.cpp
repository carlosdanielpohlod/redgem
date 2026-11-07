#include "storage/durable_file.hpp"

#include <cerrno>
#include <cstdint>
#include <system_error>

#include "core/log.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOGDI   // raylib defines its own Rectangle
#define NOUSER  // ... and CloseWindow, ShowCursor
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace redgem::durable {
namespace {

constexpr const char* kPartialSuffix = ".part";  // of a file still being written
constexpr const char* kThisFolder = ".";         // where a file named with no folder is

#ifdef _WIN32
// Bytes as they are: a save written on Windows reads the same everywhere.
constexpr const wchar_t* kWriteBytes = L"wb";
// A file made new only (it fails when the name is taken, by a link too), the link itself opened
// rather than what it points to should one appear.
constexpr DWORD kNewFileAttributes = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT;
constexpr DWORD kNoSharing = 0;
#else
constexpr const char* kWriteBytes = "wb";
// A file made new only: open fails when the name is taken, by a symbolic link too, which is never
// followed. (Not handed to the programs the engine starts, either.)
constexpr int kNewFileFlags = O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC;
constexpr mode_t kNewFileMode = 0666;  // less the user's umask, as fopen makes files
#endif

std::FILE* open_for_writing(const std::filesystem::path& file) {
#ifdef _WIN32
  return _wfopen(file.c_str(), kWriteBytes);  // (a wide name: any user's folder opens)
#else
  return std::fopen(file.c_str(), kWriteBytes);
#endif
}

#ifdef _WIN32
std::FILE* stream_of_handle(HANDLE handle) {
  const int descriptor = _open_osfhandle(reinterpret_cast<std::intptr_t>(handle), _O_BINARY);
  if (descriptor < 0) {
    CloseHandle(handle);
    return nullptr;
  }
  std::FILE* stream = _wfdopen(descriptor, kWriteBytes);
  if (stream == nullptr) _close(descriptor);
  return stream;
}

std::FILE* create_new(const std::filesystem::path& file) {
  const HANDLE handle =
      CreateFileW(file.c_str(), GENERIC_WRITE, kNoSharing, nullptr, CREATE_NEW, kNewFileAttributes, nullptr);
  return handle == INVALID_HANDLE_VALUE ? nullptr : stream_of_handle(handle);
}
#else
std::FILE* create_new(const std::filesystem::path& file) {
  const int descriptor = open(file.c_str(), kNewFileFlags, kNewFileMode);
  if (descriptor < 0) return nullptr;
  std::FILE* stream = fdopen(descriptor, kWriteBytes);
  if (stream == nullptr) close(descriptor);
  return stream;
}
#endif

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

#ifdef _WIN32
// Why a file could not be made new (CreateFileW says it in the thread's last error).
std::error_code creation_error() { return {static_cast<int>(GetLastError()), std::system_category()}; }
#else
std::error_code creation_error() { return {errno, std::generic_category()}; }
#endif

// Why a write failed: what the system said, else that not all of it was written.
std::error_code write_error() {
  const int said = errno;
  return said != 0 ? std::error_code(said, std::generic_category()) : std::make_error_code(std::errc::io_error);
}

bool wrote_all(std::FILE* out, std::string_view text) {
  return std::fwrite(text.data(), 1, text.size(), out) == text.size();
}

// Writes all of `text` into the open file, flushes it and closes it. False when not all of it
// reached where `flush` pushes it.
bool write_and_close(std::FILE* out, std::string_view text, const Flush& flush) {
  const bool flushed = wrote_all(out, text) ? flush(out) : false;
  // A full disk can show only here: the file being replaced must not be lost to half a one.
  if (std::fclose(out) != 0) return false;
  return flushed;
}

std::filesystem::path folder_of(const std::filesystem::path& file) {
  return file.has_parent_path() ? file.parent_path() : std::filesystem::path(kThisFolder);
}

// Removes whatever has the name: a file, or a link itself (never what it points to).
void remove_entry(const std::filesystem::path& file) {
  std::error_code ignored;
  std::filesystem::remove(file, ignored);
}

// Writes `text` flushed into a file made new at `partial`; why it could not.
std::error_code write_new(const std::filesystem::path& partial, std::string_view text, const Flush& flush) {
  remove_entry(partial);
  errno = 0;
  std::FILE* out = create_new(partial);
  if (out == nullptr) return creation_error();
  errno = 0;
  return write_and_close(out, text, flush) ? std::error_code() : write_error();
}

// Writes `partial` flushed and moves it over `file`; else removes it. Why it could not.
std::error_code put_in_place(const std::filesystem::path& file, const std::filesystem::path& partial,
                             std::string_view text, const Flush& flush) {
  std::error_code error = write_new(partial, text, flush);
  if (!error) std::filesystem::rename(partial, file, error);
  if (error) remove_entry(partial);
  return error;
}

}  // namespace

bool flush_to_disk(std::FILE* file) {
  if (std::fflush(file) != 0) return false;
  return sync_to_disk(file) == 0;
}

bool write_flushed(const std::filesystem::path& file, const std::string& text, const Flush& flush) {
  std::FILE* out = open_for_writing(file);
  if (out == nullptr) return false;
  return write_and_close(out, text, flush);
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
  std::filesystem::path partial = file;
  partial += kPartialSuffix;
  return replace_file_via(file, partial, text, flush);
}

bool replace_file_via(const std::filesystem::path& file, const std::filesystem::path& partial, std::string_view text,
                      const Flush& flush) {
  if (put_in_place(file, partial, text, flush)) return false;
  // (The file is replaced either way: only a power cut in the next moments could undo it.)
  if (!sync_folder(folder_of(file))) log::warning("the folder of " + file.string() + " could not be flushed to disk");
  return true;
}

bool replace_file_quickly(const std::filesystem::path& file, const std::filesystem::path& partial,
                          std::string_view text, std::string& error) {
  const std::error_code failure = put_in_place(file, partial, text, flush_to_system);
  if (failure) error = failure.message();
  return !failure;
}

}  // namespace redgem::durable
