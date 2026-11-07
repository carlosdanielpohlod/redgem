#pragma once
#include <array>
#include <cstddef>
#include <string>
#include <optional>
#include <filesystem>

namespace redgem {
inline constexpr const char* kBehaviourClass = "Behaviour";
inline constexpr std::size_t kReloadSourceBytes = 256 * 1024;  // each editor reload reads a bounded file
inline constexpr std::size_t kScriptStampBytes = 32;           // SHA-256, not filesystem timestamp precision
using ScriptStamp = std::array<unsigned char, kScriptStampBytes>;
struct ReloadFileInfo {
  std::uintmax_t bytes = 0;
  std::filesystem::file_time_type modified{};
  bool operator==(const ReloadFileInfo& other) const { return bytes == other.bytes && modified == other.modified; }
  bool operator!=(const ReloadFileInfo& other) const { return !(*this == other); }
};
struct ReloadSource {
  std::optional<ScriptStamp> observed;
  std::optional<ScriptStamp> candidate;
  std::optional<ReloadFileInfo> info;
  std::size_t polls = 0;
  bool settling = false;
  bool failed = false;
};
ScriptStamp script_stamp(const std::string& source);
}  // namespace redgem
