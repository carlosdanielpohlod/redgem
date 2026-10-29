#pragma once

#include <memory>

#include "project/editor_bridge.hpp"

namespace redgem {
class EditorApp;
// The returned bridge must outlive app.run(). The hook never runs from another thread.
std::unique_ptr<tools::EditorBridge> enable_editor_bridge(EditorApp& app, tools::EditorBridgeClock clock = {});
}  // namespace redgem
