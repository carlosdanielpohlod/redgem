// (Built only with REDGEM_RUBY_EVAL: see ruby_eval.hpp and src/CMakeLists.txt.)
#include "project/ruby_eval.hpp"

#include <mruby.h>
#include <mruby/string.h>

#include <chrono>
#include <string>

#include "app/engine.hpp"
#include "project/editor_bridge_wire.hpp"

namespace redgem::tools {
namespace {

constexpr const char* kEvalFileName = "(game_eval)";  // where the snippet's backtrace says it was
constexpr const char* kInspectMethod = "inspect";
constexpr const char* kTokenKey = "token";
constexpr const char* kCodeKey = "code";
constexpr const char* kTokenLine = "ruby eval token: ";
constexpr const char* kNotAString = "(inspect did not return a String)";
constexpr const char* kBadToken = "game_eval needs the token the editor printed when it started with --ruby-eval";
constexpr const char* kNeedsCode = "game_eval needs code, a string of Ruby";
constexpr const char* kNotPlaying = "the editor is not playing; start the game with editor action=play first";
constexpr const char* kNotEnabled =
    "game_eval is off in this editor; start it with `redgem <game> --ruby-eval`, which prints the token to give";

constexpr const char* kSchema =
    R"JSON({"name":"game_eval","description":"Evaluate Ruby in the game the open editor plays (start it with editor action=play) and get back the result's inspect, or the error with its backtrace. A development tool: the editor must have been started with --ruby-eval, which prints the token to pass. The code runs on the game's script budget.","inputSchema":{"type":"object","additionalProperties":false,"required":["token","code"],"properties":{"token":{"type":"string","minLength":1,"maxLength":128},"code":{"type":"string","minLength":1}}}})JSON";

// Whether the interpreter's own budget is one an evaluation may run on.
bool usable_budget(std::chrono::milliseconds budget) { return budget.count() > 0 && budget <= kMostEvalBudget; }

// Runs the evaluation on the interpreter's budget, capped at kMostEvalBudget, and gives it back
// its own after.
class EvalBudget {
 public:
  explicit EvalBudget(RubyVm& vm) : vm_(vm), own_(vm.budget()) {
    if (!usable_budget(own_)) vm_.set_budget(kMostEvalBudget);
  }
  ~EvalBudget() { vm_.set_budget(own_); }
  EvalBudget(const EvalBudget&) = delete;
  EvalBudget& operator=(const EvalBudget&) = delete;

 private:
  RubyVm& vm_;
  std::chrono::milliseconds own_;
};

std::string string_of(mrb_value value) {
  if (!mrb_string_p(value)) return kNotAString;
  return {RSTRING_PTR(value), static_cast<std::size_t>(RSTRING_LEN(value))};
}

// The value inspected, kept from the collector while its inspect runs.
EvalReply inspected(RubyVm& vm, mrb_value value) {
  mrb_gc_protect(vm.state(), value);
  const RubyResult shown = vm.call(value, kInspectMethod);
  if (!shown.ok) return {false, shown.error};
  return {true, string_of(shown.value)};
}

bool has_string(const mcp::Json& args, const char* key) {
  return args.is_object() && args.contains(key) && args[key].is_string();
}

// Whether the call gives the gate's token.
bool gives_token(const std::string& token, const mcp::Json& args) {
  return has_string(args, kTokenKey) && bridge::same_token(token, args[kTokenKey]);
}

// Whether the call gives code to run.
bool gives_code(const mcp::Json& args) {
  return has_string(args, kCodeKey) && !args[kCodeKey].get_ref<const std::string&>().empty();
}

// Why the call is refused before anything runs, or empty when it is not.
std::string refusal(const std::string& token, const RubyVm* vm, const mcp::Json& args) {
  if (!gives_token(token, args)) return kBadToken;
  if (!gives_code(args)) return kNeedsCode;
  if (args[kCodeKey].get_ref<const std::string&>().size() > kMostEvalCodeBytes)
    return "code is longer than " + std::to_string(kMostEvalCodeBytes) + " bytes";
  if (vm == nullptr) return kNotPlaying;
  return {};
}

}  // namespace

EvalReply evaluate_ruby(RubyVm& vm, const std::string& code) {
  const EvalBudget budget(vm);
  const int arena = mrb_gc_arena_save(vm.state());
  const RubyResult result = vm.eval(code, kEvalFileName);
  EvalReply reply = result.ok ? inspected(vm, result.value) : EvalReply{false, result.error};
  mrb_gc_arena_restore(vm.state(), arena);
  return reply;
}

mcp::Json ruby_eval_schema() {
  mcp::Json schema = mcp::Json::parse(kSchema);
  schema["inputSchema"]["properties"][kCodeKey]["maxLength"] = kMostEvalCodeBytes;
  return schema;
}

mcp::Json RubyEvalGate::answer(RubyVm* vm, const mcp::Json& args) const {
  const std::string refused = refusal(token_, vm, args);
  if (!refused.empty()) return mcp::tool_result(refused, true);
  const EvalReply reply = evaluate_ruby(*vm, args[kCodeKey].get<std::string>());
  return mcp::tool_result(reply.text, !reply.ok);
}

std::shared_ptr<const RubyEvalGate> open_ruby_eval(std::ostream& out) {
  auto gate = std::make_shared<const RubyEvalGate>(bridge::random_token());
  out << kTokenLine << gate->token() << '\n';
  return gate;
}

RubyVm* playing_vm(EditorSession& session) {
  Engine* runtime = session.runtime();
  if (session.editing() || runtime == nullptr) return nullptr;
  return &runtime->scripts().vm();
}

mcp::Json answer_ruby_eval(const RubyEvalGate* gate, EditorSession& session, const mcp::Json& args) {
  if (gate == nullptr) return mcp::tool_result(kNotEnabled, true);
  return gate->answer(playing_vm(session), args);
}

}  // namespace redgem::tools
