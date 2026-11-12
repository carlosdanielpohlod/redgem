#pragma once

#include <cstdint>
#include <filesystem>

namespace redgem {
// The OS releases this lease on exit or crash. Its inode must never be removed while in use.
class RecoveryLease {
 public:
  RecoveryLease() = default;
  ~RecoveryLease();
  RecoveryLease(const RecoveryLease&) = delete;
  RecoveryLease& operator=(const RecoveryLease&) = delete;
  enum class Result { Acquired, Busy, Unavailable };
  Result acquire(const std::filesystem::path& file);
  bool held() const { return handle_ != kInvalid; }
  void release();

 private:
  static constexpr std::intptr_t kInvalid = -1;
  std::intptr_t handle_ = kInvalid;
};
}  // namespace redgem
