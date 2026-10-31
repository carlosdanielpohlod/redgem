#pragma once

#include <filesystem>
#include <functional>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "project/mcp_protocol.hpp"

namespace redgem::tools {
using EditorBridgeClock = std::function<std::chrono::steady_clock::time_point()>;
// Opt-in local endpoint. tick runs handlers on its caller's thread, without waiting for I/O.
class EditorBridge {
 public:
  explicit EditorBridge(const std::filesystem::path& game, const std::filesystem::path& private_root = {},
                        EditorBridgeClock clock = {});
  ~EditorBridge();
  EditorBridge(const EditorBridge&) = delete;
  EditorBridge& operator=(const EditorBridge&) = delete;
  const std::filesystem::path& connection_file() const;
  void tick(const mcp::Invoke& invoke);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

// One bounded request, with no automatic retry or fallback to file edits on failure.
mcp::Json call_editor(const std::filesystem::path& game, const std::filesystem::path& connection,
                      const std::string& name, const mcp::Json& args, EditorBridgeClock clock = {});
// Why a try reached no editor. Either way nothing but a random nonce was sent, so another editor
// or the files may answer instead without doing the request twice.
enum class EditorMiss {
  None,      // an editor answered (perhaps with an error, or it timed out after the request)
  Refused,   // nothing listens on the port: the editor is gone
  Unproven,  // something listens but did not prove it holds the connection's credential
};
struct EditorAttempt {
  std::optional<mcp::Json> reply;  // the answer, when miss is None
  EditorMiss miss = EditorMiss::None;
};
// The same as call_editor, but saying so instead of answering when no editor took the request.
EditorAttempt try_editor(const std::filesystem::path& game, const std::filesystem::path& connection,
                         const std::string& name, const mcp::Json& args, EditorBridgeClock clock = {});
// Removes a connection file whose editor is gone, with the private folder it was written in.
void forget_connection(const std::filesystem::path& connection);
// The connection files editors of `game` wrote under `private_root` (the user's RedGem data folder
// when empty), newest first. Stale files of editors that crashed are among them.
std::vector<std::filesystem::path> editor_connections(const std::filesystem::path& game,
                                                      const std::filesystem::path& private_root = {});
}  // namespace redgem::tools
