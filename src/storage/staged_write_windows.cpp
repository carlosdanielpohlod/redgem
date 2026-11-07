// staging::write_staged on Windows: each write's own file, made new beside the file it replaces,
// held open and shared with nobody (no other program opens, moves or removes it meanwhile), and
// moved over that file by its handle. Whatever has its name by then, the file moved is the one
// written.
#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define NOGDI   // raylib defines its own Rectangle
#define NOUSER  // ... and CloseWindow, ShowCursor
#include <windows.h>
#include <fcntl.h>
#include <io.h>

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <string>
#include <system_error>
#include <vector>

#include "storage/staged_write.hpp"

namespace redgem::durable::staging {
namespace {

// Bytes as they are: a file written on Windows reads the same everywhere.
constexpr const wchar_t* kWriteBytes = L"wb";
constexpr DWORD kWriteAndMove = GENERIC_WRITE | DELETE;
constexpr DWORD kNoSharing = 0;
// A file made new only (it fails when the name is taken, by a link too), the link itself opened
// rather than what it points to should one appear.
constexpr DWORD kNewFileAttributes = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT;
constexpr DWORD kSameAccess = 0;  // (DUPLICATE_SAME_ACCESS says it)

std::string system_said(DWORD error) {
  return std::error_code(static_cast<int>(error), std::system_category()).message();
}

bool name_taken(DWORD error) { return error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS; }

// The write's own file beside `file`, made new under a name no other write has; INVALID_HANDLE_VALUE,
// with why, when it cannot be.
HANDLE create_partial(const std::filesystem::path& file, const char* suffix, std::string& error) {
  for (int tries = 0; tries < kMostNameTries; ++tries) {
    const std::filesystem::path partial = folder_of(file) / staged_name(file, suffix);
    const HANDLE handle =
        CreateFileW(partial.c_str(), kWriteAndMove, kNoSharing, nullptr, CREATE_NEW, kNewFileAttributes, nullptr);
    if (handle != INVALID_HANDLE_VALUE) return handle;
    if (!name_taken(GetLastError())) break;
  }
  error = system_said(GetLastError());
  return INVALID_HANDLE_VALUE;
}

// A second handle on the same open file, kept to move or remove it once the stream is closed.
HANDLE kept_handle(HANDLE handle) {
  HANDLE kept = INVALID_HANDLE_VALUE;
  const HANDLE self = GetCurrentProcess();
  const bool made = DuplicateHandle(self, handle, self, &kept, kSameAccess, FALSE, DUPLICATE_SAME_ACCESS) != 0;
  return made ? kept : INVALID_HANDLE_VALUE;
}

// Removes the open file by its handle: it goes when its last handle is closed.
void remove_by_handle(HANDLE handle) {
  FILE_DISPOSITION_INFO disposition{};
  disposition.DeleteFile = TRUE;
  SetFileInformationByHandle(handle, FileDispositionInfo, &disposition, sizeof(disposition));
}

// Writes the text, flushed, through the handle, and closes it (the kept handle stays open).
std::string write_through(HANDLE handle, std::string_view text, const Flush& flush) {
  const int descriptor = _open_osfhandle(reinterpret_cast<std::intptr_t>(handle), _O_BINARY);
  if (descriptor < 0) {
    CloseHandle(handle);
    return write_error().message();
  }
  std::FILE* out = _wfdopen(descriptor, kWriteBytes);
  if (out == nullptr) {
    _close(descriptor);
    return write_error().message();
  }
  errno = 0;
  return write_and_close(out, text, flush) ? std::string() : write_error().message();
}

// Moves the open file over `file`, by its handle.
std::string move_by_handle(HANDLE handle, const std::filesystem::path& file) {
  std::error_code fs_error;
  const std::wstring target = std::filesystem::absolute(file, fs_error).wstring();
  if (fs_error) return fs_error.message();
  // (FILE_RENAME_INFO ends in the name; its one character of room holds the closing NUL.)
  std::vector<std::uint8_t> buffer(sizeof(FILE_RENAME_INFO) + target.size() * sizeof(wchar_t));
  auto* info = reinterpret_cast<FILE_RENAME_INFO*>(buffer.data());
  info->ReplaceIfExists = TRUE;
  info->RootDirectory = nullptr;
  info->FileNameLength = static_cast<DWORD>(target.size() * sizeof(wchar_t));
  std::memcpy(info->FileName, target.c_str(), info->FileNameLength);
  const auto size = static_cast<DWORD>(buffer.size());
  if (SetFileInformationByHandle(handle, FileRenameInfo, info, size) != 0) return {};
  return system_said(GetLastError());
}

// Writes through `handle` and moves the file by `kept`; removes it when it is not moved.
std::string write_and_move(HANDLE handle, HANDLE kept, const std::filesystem::path& file, std::string_view text,
                           const Flush& flush) {
  std::string error = write_through(handle, text, flush);
  if (error.empty()) error = move_by_handle(kept, file);
  if (!error.empty()) remove_by_handle(kept);
  CloseHandle(kept);
  return error;
}

}  // namespace

std::string write_staged(const std::filesystem::path& file, const char* suffix, std::string_view text,
                         const Flush& flush) {
  std::string error;
  const HANDLE handle = create_partial(file, suffix, error);
  if (handle == INVALID_HANDLE_VALUE) return error;
  const HANDLE kept = kept_handle(handle);
  if (kept != INVALID_HANDLE_VALUE) return write_and_move(handle, kept, file, text, flush);
  error = system_said(GetLastError());
  remove_by_handle(handle);
  CloseHandle(handle);
  return error;
}

}  // namespace redgem::durable::staging

#endif
