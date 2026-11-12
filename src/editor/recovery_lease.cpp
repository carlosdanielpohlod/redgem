#include "editor/recovery_lease.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace redgem {
RecoveryLease::~RecoveryLease() { release(); }
#ifdef _WIN32
namespace {
RecoveryLease::Result lock_handle(HANDLE handle) {
  BY_HANDLE_FILE_INFORMATION info{};
  OVERLAPPED position{};
  const bool ordinary =
      GetFileInformationByHandle(handle, &info) && !(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT);
  using Result = RecoveryLease::Result;
  if (!ordinary) return Result::Unavailable;
  if (LockFileEx(handle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &position))
    return Result::Acquired;
  return GetLastError() == ERROR_LOCK_VIOLATION ? Result::Busy : Result::Unavailable;
}
}  // namespace
RecoveryLease::Result RecoveryLease::acquire(const std::filesystem::path& file) {
  release();
  const auto handle = CreateFileW(file.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  nullptr, OPEN_ALWAYS, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
  if (handle == INVALID_HANDLE_VALUE) return Result::Unavailable;
  handle_ = reinterpret_cast<std::intptr_t>(handle);
  const auto result = lock_handle(handle);
  if (result != Result::Acquired) release();
  return result;
}
void RecoveryLease::release() {
  if (held()) CloseHandle(reinterpret_cast<HANDLE>(handle_));
  handle_ = kInvalid;
}
#else
RecoveryLease::Result RecoveryLease::acquire(const std::filesystem::path& file) {
  release();
  handle_ = open(file.c_str(), O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, S_IRUSR | S_IWUSR);
  if (!held()) return Result::Unavailable;
  struct stat info{};
  const auto descriptor = static_cast<int>(handle_);
  const bool ordinary = fstat(descriptor, &info) == 0 && S_ISREG(info.st_mode);
  if (ordinary && flock(descriptor, LOCK_EX | LOCK_NB) == 0) return Result::Acquired;
  const bool busy = ordinary && errno == EWOULDBLOCK;
  release();
  return busy ? Result::Busy : Result::Unavailable;
}
void RecoveryLease::release() {
  if (held()) close(static_cast<int>(handle_));
  handle_ = kInvalid;
}
#endif
}  // namespace redgem
