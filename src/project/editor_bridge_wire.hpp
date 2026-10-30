#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include "net/socket/socket.hpp"
#include "project/mcp_protocol.hpp"
#include "project/editor_bridge.hpp"
#include <utility>

namespace redgem::tools::bridge {
using mcp::Json;
using Bytes = std::vector<std::uint8_t>;
using Clock = std::chrono::steady_clock;
inline constexpr std::size_t kMaxRequest = mcp::kMaxRequestBytes;
inline constexpr std::size_t kMaxReply = 256 * 1024;
// A supplied clock isolates transport deadlines from slow graphics in system tests. Production
// callers omit it and keep the same real steady clock and bounded request deadline.
inline EditorBridgeClock clock_or_steady(EditorBridgeClock clock) {
  return clock ? std::move(clock) : EditorBridgeClock(Clock::now);
}
inline constexpr auto kTimeout = std::chrono::seconds(5);
inline constexpr float kPollSeconds = 0.005f;
inline constexpr std::size_t kMaxClients = 4;
inline constexpr int kWireVersion = 1;

struct Address {
  std::filesystem::path game;
  std::uint16_t port = 0;
  std::string token;
};

class ConnectionFile {
 public:
  ConnectionFile(const Address& address, const std::filesystem::path& parent);
  ~ConnectionFile();
  ConnectionFile(const ConnectionFile&) = delete;
  ConnectionFile& operator=(const ConnectionFile&) = delete;
  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path directory_;
  std::filesystem::path path_;
};
std::string random_token();
bool same_token(const std::string& expected, const Json& given);
Address read_address(const std::filesystem::path& game, const std::filesystem::path& path);
// Every connection file under `root` (the user's bridge folder when empty), newest first.
std::vector<std::filesystem::path> connection_files(const std::filesystem::path& root);
// Whether a connection file is a valid one for `game`.
bool belongs_to(const std::filesystem::path& game, const std::filesystem::path& file);
Json parse(const std::string& text);
Bytes encode(const Json& value);
bool complete(const Bytes& bytes);
}  // namespace redgem::tools::bridge
