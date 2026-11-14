// (Built only with REDGEM_RUBY_EVAL: see ruby_eval.hpp and src/CMakeLists.txt.)
#include "project/ruby_eval.hpp"

#include <mruby.h>
#include <mruby/string.h>

#include <chrono>
#include <string>

#include "app/engine.hpp"
#include "project/editor_live_tools.hpp"
#include "project/editor_bridge_wire.hpp"
#include "script/script_config.hpp"

namespace redgem::tools {
namespace {

constexpr const char* kEvalFileName = "(game_eval)";  // where the snippet's backtrace says it was
// The snippet, and its result inspected, in one call (and so on one budget). Its first line stays
// the first: the lines its errors name are its own.
constexpr const char* kWrapBefore = "begin;";
constexpr const char* kWrapAfter = "\nend.inspect";
constexpr const char* kTokenKey = "token";
constexpr const char* kCodeKey = "code";
constexpr const char* kTokenLine = "ruby eval token: ";
constexpr const char* kNotAString = "(inspect did not return a String)";
constexpr const char* kBadToken = "game_eval needs the token the editor printed when it started with --ruby-eval";
constexpr const char* kNeedsCode = "game_eval needs code, a string of Ruby";
constexpr const char* kNoEvalEditor =
    "no editor of this game is running with --ruby-eval; start one with `redgem <game> --ruby-eval`";
constexpr const char* kNotEnabled =
    "game_eval is off in this editor; start it with `redgem <game> --ruby-eval`, which prints the token to give";

constexpr const char* kSchema =
    R"JSON({"name":"game_eval","description":"Evaluate Ruby in the game the open editor plays (start it with editor action=play) and get back the result's inspect, or the error with its backtrace. A development tool: the editor must have been started with --ruby-eval, which prints the token to pass. The code runs on the game's script budget.","inputSchema":{"type":"object","additionalProperties":false,"required":["token","code"],"properties":{"token":{"type":"string","minLength":1,"maxLength":128},"code":{"type":"string","minLength":1}}}})JSON";

// The longest an evaluation keeps the editor: its call and the description of its error, each
// with the engine's work forgiven and the grace to unwind.
constexpr int kEvalCalls = 2;  // the evaluation, and the description of its error
constexpr auto kMostEvalTime = kEvalCalls * ((1 + config::kScriptForgivenBudgets) * kMostEvalBudget +
                                             std::chrono::milliseconds(config::kScriptGraceMs));
static_assert(kMostEvalTime < bridge::kTimeout, "an evaluation must answer before the bridge's caller gives up");

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

// Whether the call gives the gate's token.
bool gives_token(const std::string& token, const mcp::Json& args) {
  return args.is_object() && bridge::same_token(token, args.value(kTokenKey, mcp::Json()));
}

// Whether the call gives code to run.
bool gives_code(const mcp::Json& args) {
  const mcp::Json code = args.value(kCodeKey, mcp::Json());
  return code.is_string() && !code.get_ref<const std::string&>().empty();
}

// Why the call is refused before anything runs, or empty when it is not.
std::string refusal(const std::string& token, const RubyVm* vm, const mcp::Json& args) {
  if (!gives_token(token, args)) return kBadToken;
  if (!gives_code(args)) return kNeedsCode;
  if (args[kCodeKey].get_ref<const std::string&>().size() > kMostEvalCodeBytes)
    return "code is longer than " + std::to_string(kMostEvalCodeBytes) + " bytes";
  if (vm == nullptr) return kEditorNotPlaying;
  return {};
}

}  // namespace

EvalReply evaluate_ruby(RubyVm& vm, const std::string& code) {
  const EvalBudget budget(vm);
  // (The inspect is read at once: nothing runs that could collect it.)
  const RubyResult result = vm.eval(kWrapBefore + code + kWrapAfter, kEvalFileName);
  if (!result.ok) return {false, result.error};
  return {true, string_of(result.value)};
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

mcp::Json answer_ruby_eval(const RubyEvalGate* gate, EditorSession& session, const mcp::Json& args) {
  if (gate == nullptr) return mcp::tool_result(kNotEnabled, true);
  Engine* runtime = playing_runtime(session);
  return gate->answer(runtime == nullptr ? nullptr : &runtime->scripts().vm(), args);
}

mcp::Json no_editor_for_ruby_eval() { return mcp::tool_result(kNoEvalEditor, true); }

}  // namespace redgem::tools
