#include "editor/recovery_lease.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace redgem {
RecoveryLease::~RecoveryLease() { release(); }
#ifdef _WIN32
namespace {
bool lock_handle(HANDLE handle) {
  BY_HANDLE_FILE_INFORMATION info{};
  OVERLAPPED position{};
  const bool ordinary =
      GetFileInformationByHandle(handle, &info) && !(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT);
  return ordinary && LockFileEx(handle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &position);
}
}  // namespace
bool RecoveryLease::acquire(const std::filesystem::path& file) {
  release();
  const auto handle = CreateFileW(file.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  nullptr, OPEN_ALWAYS, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
  if (handle == INVALID_HANDLE_VALUE) return false;
  handle_ = reinterpret_cast<std::intptr_t>(handle);
  const bool locked = lock_handle(handle);
  if (!locked) release();
  return locked;
}
void RecoveryLease::release() {
  if (held()) CloseHandle(reinterpret_cast<HANDLE>(handle_));
  handle_ = kInvalid;
}
#else
bool RecoveryLease::acquire(const std::filesystem::path& file) {
  release();
  handle_ = open(file.c_str(), O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, S_IRUSR | S_IWUSR);
  if (!held()) return false;
  struct stat info{};
  const auto descriptor = static_cast<int>(handle_);
  const bool ordinary = fstat(descriptor, &info) == 0 && S_ISREG(info.st_mode);
  const bool locked = ordinary && flock(descriptor, LOCK_EX | LOCK_NB) == 0;
  if (!locked) release();
  return locked;
}
void RecoveryLease::release() {
  if (held()) close(static_cast<int>(handle_));
  handle_ = kInvalid;
}
#endif
}  // namespace redgem
