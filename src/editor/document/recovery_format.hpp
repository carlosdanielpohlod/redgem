#pragma once

#include <cstddef>

namespace redgem::recovery_format {
inline constexpr const char* kMetadata = "redgem_editor_recovery";
// Bound synchronous encoding and reopening on the editing thread.
inline constexpr std::size_t kMaxBytes = 64 * 1024 * 1024;
}  // namespace redgem::recovery_format
