#include "project/editor_bridge_run.hpp"

#include <iostream>

#include "core/log.hpp"
#include "editor/editor_app.hpp"
#include "project/editor_live_tools.hpp"

namespace redgem {
namespace {
void hook(EditorApp& app, tools::EditorBridge* server) {
  app.set_frame_hook([server](EditorApp& editor) {
    server->tick([&editor](const std::string& name, const tools::mcp::Json& args) {
      return tools::invoke_editor(editor.session(), name, args, editor.interaction_active());
    });
  });
}
}  // namespace

std::unique_ptr<tools::EditorBridge> enable_editor_bridge(EditorApp& app) {
  try {
    auto bridge = std::make_unique<tools::EditorBridge>(app.session().game_dir());
    std::cerr << "editor bridge connection: " << bridge->connection_file().string() << '\n';
    hook(app, bridge.get());
    return bridge;
  } catch (const std::exception&) {
    log::error("could not enable the local editor bridge");
    return nullptr;
  }
}
}  // namespace redgem
