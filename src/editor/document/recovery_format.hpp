#pragma once

#include <cstddef>
#include <string>

namespace redgem::recovery_format {
inline constexpr const char* kMetadata = "redgem_editor_recovery";
// Bound synchronous encoding and reopening on the editing thread.
inline constexpr std::size_t kBytesPerMiB = 1024 * 1024;
inline constexpr std::size_t kMaxBytes = 64 * kBytesPerMiB;
inline std::string limit_error() {
  return "scene recovery exceeds the " + std::to_string(kMaxBytes / kBytesPerMiB) + " MiB limit";
}
}  // namespace redgem::recovery_format
