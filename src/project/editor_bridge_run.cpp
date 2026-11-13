#include "project/editor_bridge_run.hpp"

#include <iostream>
#include <memory>
#include <utility>

#include "core/log.hpp"
#include "editor/editor_app.hpp"
#include "project/editor_live_tools.hpp"
#include "project/editor_window_tools.hpp"
#include "project/ruby_eval.hpp"

namespace redgem {
namespace {
tools::mcp::Invoke window_for(EditorApp& editor) {
  return [&editor](const std::string& action, const tools::mcp::Json& values) {
    return tools::editor_window_tool(editor, action, values);
  };
}

// The evaluation gate of an editor started with --ruby-eval (project/ruby_eval.hpp).
#if defined(REDGEM_RUBY_EVAL)
using EvalGate = std::shared_ptr<const tools::RubyEvalGate>;
#else
struct EvalGate {};
#endif

tools::mcp::Json answer(EditorApp& editor, const EvalGate& gate, const std::string& name,
                        const tools::mcp::Json& args) {
#if defined(REDGEM_RUBY_EVAL)
  if (tools::is_ruby_eval_tool(name)) return tools::answer_ruby_eval(gate.get(), editor.session(), args);
#else
  (void)gate;
#endif
  return tools::invoke_editor(editor.session(), name, args, editor.interaction_active(), window_for(editor));
}

void hook(EditorApp& app, tools::EditorBridge* server, EvalGate gate) {
  app.set_frame_hook([server, gate = std::move(gate)](EditorApp& editor) {
    server->tick([&editor, &gate](const std::string& name, const tools::mcp::Json& args) {
      return answer(editor, gate, name, args);
    });
  });
}

// The evaluation gate for an editor started with --ruby-eval (none otherwise, or in a build
// without it).
EvalGate eval_gate(bool ruby_eval) {
#if defined(REDGEM_RUBY_EVAL)
  if (ruby_eval) return tools::open_ruby_eval(std::cerr);
#else
  (void)ruby_eval;
#endif
  return {};
}
}  // namespace

std::unique_ptr<tools::EditorBridge> enable_editor_bridge(EditorApp& app, tools::EditorBridgeClock clock,
                                                          bool ruby_eval) {
  try {
    auto bridge =
        std::make_unique<tools::EditorBridge>(app.session().game_dir(), std::filesystem::path{}, std::move(clock));
    std::cerr << "editor bridge connection: " << bridge->connection_file().string() << '\n';
    hook(app, bridge.get(), eval_gate(ruby_eval));
    return bridge;
  } catch (const std::exception&) {
    log::error("could not enable the local editor bridge");
    return nullptr;
  }
}
}  // namespace redgem
