#pragma once

// Ruby evaluated in the game an editor plays, for developers and their assistants: a snippet in,
// its result's inspect (or the error with its backtrace) out.
//
// It is there only when everything says so:
// - the build: the CMake option REDGEM_RUBY_EVAL, on in development builds and off in release
//   builds (it follows REDGEM_BUILD_TESTS), which leave out this file's code and every text of
//   it (tests/ships_no_ruby_eval.sh looks);
// - the editor: started with --ruby-eval, which opens its local bridge (localhost only, see
//   editor_bridge.hpp) and prints a random token;
// - the call: it gives that token, and code no longer than kMostEvalCodeBytes; the answer is cut
//   at 8 KiB, as any tool's.
// The code runs on the game's script budget (script/script_budget.hpp), like any call the engine
// makes into the scripts: a snippet that never returns is ended with a ScriptTimeout.

#include <string>

#if defined(REDGEM_RUBY_EVAL)
#include <chrono>
#include <cstddef>
#include <memory>
#include <ostream>
#include <utility>

#include "editor/editor_session.hpp"
#include "project/mcp_protocol.hpp"
#include "script/ruby_vm.hpp"
#endif

namespace redgem::tools {

#if defined(REDGEM_RUBY_EVAL)
inline constexpr const char* kRubyEvalTool = "game_eval";
#endif

// Whether `name` is the evaluation tool (never, in a build without it).
inline bool is_ruby_eval_tool(const std::string& name) {
#if defined(REDGEM_RUBY_EVAL)
  return name == kRubyEvalTool;
#else
  (void)name;
  return false;
#endif
}

#if defined(REDGEM_RUBY_EVAL)
// The most code one evaluation takes, in bytes.
inline constexpr std::size_t kMostEvalCodeBytes = 16 * 1024;
// The longest an evaluation runs: the game's script budget when it is shorter, this when it is
// longer or the game has none. (The bridge's caller gives up after five seconds.)
inline constexpr std::chrono::milliseconds kMostEvalBudget{2000};

struct EvalReply {
  bool ok = false;
  std::string text;  // the result inspected, or the error and its backtrace
};

// Evaluates `code` at the top level of the interpreter, on its budget (at most kMostEvalBudget,
// its own set back after). (The tool's answer is cut as every tool's text is: mcp::tool_result.)
EvalReply evaluate_ruby(RubyVm& vm, const std::string& code);

// The tool as the MCP catalog lists it.
mcp::Json ruby_eval_schema();

// Answers the tool to callers that give its token. `vm` is the playing game's interpreter, or
// null when nothing plays.
class RubyEvalGate {
 public:
  explicit RubyEvalGate(std::string token) : token_(std::move(token)) {}
  mcp::Json answer(RubyVm* vm, const mcp::Json& args) const;
  const std::string& token() const { return token_; }

 private:
  std::string token_;
};

// A gate with a fresh random token, which it prints to `out` for the developer to hand on.
std::shared_ptr<const RubyEvalGate> open_ruby_eval(std::ostream& out);

// The interpreter of the game the editor plays, or null when it is editing.
RubyVm* playing_vm(EditorSession& session);

// The tool in an editor: through `gate` when it was started with --ruby-eval; without one (null),
// a refusal saying how to turn it on.
mcp::Json answer_ruby_eval(const RubyEvalGate* gate, EditorSession& session, const mcp::Json& args);
#endif

}  // namespace redgem::tools
