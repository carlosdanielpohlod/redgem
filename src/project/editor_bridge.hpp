#pragma once

#include <filesystem>
#include <functional>
#include <chrono>
#include <memory>

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
}  // namespace redgem::tools
