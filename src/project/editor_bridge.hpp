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
// The same, but nothing when no editor answers at that connection (it is not running, or the file
// is gone or stale), so that the caller can try another editor or work on the files instead.
std::optional<mcp::Json> try_editor(const std::filesystem::path& game, const std::filesystem::path& connection,
                                    const std::string& name, const mcp::Json& args, EditorBridgeClock clock = {});
// The connection files editors of `game` wrote under `private_root` (the user's RedGem data folder
// when empty), newest first. Stale files of editors that crashed are among them.
std::vector<std::filesystem::path> editor_connections(const std::filesystem::path& game,
                                                      const std::filesystem::path& private_root = {});
}  // namespace redgem::tools
