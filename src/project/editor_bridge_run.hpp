#pragma once

#include <memory>

#include "project/editor_bridge.hpp"

namespace redgem {
class EditorApp;
// The returned bridge must outlive app.run(). The hook never runs from another thread. With
// `ruby_eval` (--ruby-eval, in a build that has it) the bridge also answers game_eval, to callers
// that give the token it prints (project/ruby_eval.hpp).
std::unique_ptr<tools::EditorBridge> enable_editor_bridge(EditorApp& app, tools::EditorBridgeClock clock = {},
                                                          bool ruby_eval = false);
}  // namespace redgem
