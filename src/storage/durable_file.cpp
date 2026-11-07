#include "storage/durable_file.hpp"

#include <cerrno>
#include <cstdint>
#include <random>
#include <sstream>
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
constexpr char kNameSeparator = '.';             // before the random part of a partial file's name
// Names tried for a partial file before giving up. Each is random: a name already taken is a sign
// of something else at work in the folder, never of bad luck.
constexpr int kMostNameTries = 8;
constexpr int kBitsInAWord = 32;
constexpr const char* kPartialReplaced =
    "the file being written was replaced by another while it was written; it was left as it is";

#ifdef _WIN32
// Bytes as they are: a save written on Windows reads the same everywhere.
constexpr const wchar_t* kWriteBytes = L"wb";
// A file made new only (it fails when the name is taken, by a link too), the link itself opened
// rather than what it points to should one appear.
constexpr DWORD kNewFileAttributes = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT;
constexpr DWORD kNoSharing = 0;
// Opening a name only to ask what it is: never through a link, folders too, whoever has it open.
constexpr DWORD kOnlyAsking = 0;
constexpr DWORD kShareAll = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
constexpr DWORD kTheEntryItself = FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS;
#else
constexpr const char* kWriteBytes = "wb";
// A file made new only: open fails when the name is taken, by a symbolic link too, which is never
// followed. (Not handed to the programs the engine starts, either.)
constexpr int kNewFileFlags = O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC;
constexpr mode_t kNewFileMode = 0666;  // less the user's umask, as fopen makes files
#endif

// What a file is, whatever its name: the device it is on and its number there. Unknown when it
// could not be asked.
struct Identity {
  std::uint64_t device = 0;
  std::uint64_t file = 0;
  bool known = false;
};

bool same_file(const Identity& a, const Identity& b) {
  return a.known && b.known && a.device == b.device && a.file == b.file;
}

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

// Why a file could not be made new (CreateFileW says it in the thread's last error).
std::error_code creation_error() { return {static_cast<int>(GetLastError()), std::system_category()}; }

bool name_taken(const std::error_code& error) {
  return error.value() == ERROR_FILE_EXISTS || error.value() == ERROR_ALREADY_EXISTS;
}

Identity identity_of(const BY_HANDLE_FILE_INFORMATION& info) {
  const std::uint64_t index = (std::uint64_t{info.nFileIndexHigh} << kBitsInAWord) | info.nFileIndexLow;
  return {info.dwVolumeSerialNumber, index, true};
}

Identity identity_of_handle(HANDLE handle) {
  BY_HANDLE_FILE_INFORMATION info{};
  return GetFileInformationByHandle(handle, &info) != 0 ? identity_of(info) : Identity{};
}

Identity identity_of_stream(std::FILE* stream) {
  return identity_of_handle(reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stream))));
}

// What has the name now: the entry itself, a link or a junction too (which is never what was written).
Identity identity_at(const std::filesystem::path& path) {
  const HANDLE handle =
      CreateFileW(path.c_str(), kOnlyAsking, kShareAll, nullptr, OPEN_EXISTING, kTheEntryItself, nullptr);
  if (handle == INVALID_HANDLE_VALUE) return {};
  const Identity identity = identity_of_handle(handle);
  CloseHandle(handle);
  return identity;
}
#else
std::FILE* create_new(const std::filesystem::path& file) {
  const int descriptor = open(file.c_str(), kNewFileFlags, kNewFileMode);
  if (descriptor < 0) return nullptr;
  std::FILE* stream = fdopen(descriptor, kWriteBytes);
  if (stream == nullptr) close(descriptor);
  return stream;
}

std::error_code creation_error() { return {errno, std::generic_category()}; }

bool name_taken(const std::error_code& error) { return error.value() == EEXIST; }

Identity identity_of(const struct stat& status) {
  return {static_cast<std::uint64_t>(status.st_dev), static_cast<std::uint64_t>(status.st_ino), true};
}

Identity identity_of_stream(std::FILE* stream) {
  struct stat status{};
  return fstat(fileno(stream), &status) == 0 ? identity_of(status) : Identity{};
}

// What has the name now: the entry itself, never what a link points to; a regular file only.
Identity identity_at(const std::filesystem::path& path) {
  struct stat status{};
  if (lstat(path.c_str(), &status) != 0) return {};
  return S_ISREG(status.st_mode) ? identity_of(status) : Identity{};
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

void remove_entry(const std::filesystem::path& file) {
  std::error_code ignored;
  std::filesystem::remove(file, ignored);
}

// A name for one write's partial file that no other has: <file>.<random><suffix>. (Two writes of the
// same file, from two threads or two programs, each write and move their own.)
std::filesystem::path unique_partial(const std::filesystem::path& file, const char* suffix) {
  thread_local std::mt19937_64 random{std::random_device{}()};
  std::ostringstream name;
  name << kNameSeparator << std::hex << random();
  std::filesystem::path partial = file;
  partial += name.str();
  partial += suffix;
  return partial;
}

// Makes a partial file new, under a name of its own, and opens it; nullptr, with why, when it cannot.
std::FILE* create_partial(const std::filesystem::path& file, const char* suffix, std::filesystem::path& partial,
                          std::error_code& error) {
  for (int tries = 0; tries < kMostNameTries; ++tries) {
    partial = unique_partial(file, suffix);
    if (std::FILE* out = create_new(partial)) return out;
    error = creation_error();
    if (!name_taken(error)) return nullptr;
  }
  return nullptr;
}

// Writes `text` into the open partial file, flushed, and closes it: what the file written is, and
// why it could not be written.
std::error_code write_partial(std::FILE* out, std::string_view text, const Flush& flush, Identity& written) {
  written = identity_of_stream(out);
  errno = 0;
  return write_and_close(out, text, flush) ? std::error_code() : write_error();
}

// Writes `text` into a partial file of its own and moves it over `file`. Only that file is ever
// moved or removed: should its name lead elsewhere by the time it is moved (a link put in its place),
// nothing is touched. Why it could not; empty when it could.
std::string put_in_place(const std::filesystem::path& file, const char* suffix, std::string_view text,
                         const Flush& flush) {
  std::filesystem::path partial;
  std::error_code error;
  std::FILE* out = create_partial(file, suffix, partial, error);
  if (out == nullptr) return error.message();
  Identity written;
  error = write_partial(out, text, flush, written);
  if (!same_file(written, identity_at(partial))) return kPartialReplaced;
  if (!error) std::filesystem::rename(partial, file, error);
  if (error) remove_entry(partial);
  return error ? error.message() : std::string();
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
  return replace_file_via(file, kPartialSuffix, text, flush);
}

bool replace_file_via(const std::filesystem::path& file, const char* partial_suffix, std::string_view text,
                      const Flush& flush) {
  if (!put_in_place(file, partial_suffix, text, flush).empty()) return false;
  // (The file is replaced either way: only a power cut in the next moments could undo it.)
  if (!sync_folder(folder_of(file))) log::warning("the folder of " + file.string() + " could not be flushed to disk");
  return true;
}

bool replace_file_quickly(const std::filesystem::path& file, const char* partial_suffix, std::string_view text,
                          std::string& error) {
  error = put_in_place(file, partial_suffix, text, flush_to_system);
  return error.empty();
}

}  // namespace redgem::durable
