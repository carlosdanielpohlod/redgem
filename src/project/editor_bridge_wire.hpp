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
// How long a client waits for the listener to prove it is the editor before it sends anything
// secret. An editor answers between two frames; a silent program on a crashed editor's port
// costs no more than this (and discovery then leaves its file alone for a while).
inline constexpr auto kHandshakeTimeout = std::chrono::seconds(1);
inline constexpr float kPollSeconds = 0.005f;
inline constexpr std::size_t kMaxClients = 4;
inline constexpr int kWireVersion = 2;  // 2: the editor proves itself before a request

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
// The first line of `bytes`, which loses it (complete() says there is one).
std::string take_line(Bytes& bytes);

// The handshake. The client opens with {"hello": nonce}, a fresh random_token(); the editor
// answers {"proof": server_proof(token, nonce)}, which only a holder of the connection's
// credential can compute; only then does the client send its request with the token itself.
std::string server_proof(const std::string& token, const std::string& nonce);
// Whether a hello carries a nonce shaped as random_token() makes them.
bool valid_nonce(const Json& nonce);
}  // namespace redgem::tools::bridge
