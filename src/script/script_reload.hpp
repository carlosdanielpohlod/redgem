#pragma once
#include <array>
#include <cstddef>
#include <string>

namespace redgem {
inline constexpr std::size_t kReloadSourceBytes = 256 * 1024;  // each editor reload reads a bounded file
inline constexpr std::size_t kScriptStampBytes = 32;           // SHA-256, not filesystem timestamp precision
using ScriptStamp = std::array<unsigned char, kScriptStampBytes>;
ScriptStamp script_stamp(const std::string& source);
}  // namespace redgem
