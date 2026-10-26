#include "project/run_session.hpp"

#include <mruby/string.h>

#include <cstddef>
#include <string_view>
#include <utility>

#include "core/config.hpp"
#include "core/message_lines.hpp"
#include "core/safe_path.hpp"
#include "core/short_text.hpp"
#include "core/translations.hpp"

namespace redgem::tools {
namespace {

constexpr std::size_t kMostOutputBytes = 8192;
constexpr std::size_t kMostValueBytes = 512;
constexpr const char* kExpressionFile = "<run expression>";

std::string frame_label(long long frame) { return tr("project.run.frame_label", {{"frame", std::to_string(frame)}}); }

std::string short_value(std::string_view text) {
  const std::string_view prefix = utf8_prefix(text, kMostValueBytes);
  return printable(prefix) + (prefix.size() < text.size() ? "..." : "");
}

class RunSession {
 public:
  RunSession(Engine& engine, const RunPlan& plan) : engine_(engine), plan_(plan) {}

  RunOutcome run(long long frames) {
    while (outcome_.frames < frames && !engine_.finished() && !stopped()) step();
    if (plan_.print && !outcome_.failed && outcome_.frames % plan_.every != 0) print_value();
    if (plan_.until && !reached_ && !outcome_.failed) fail(tr("project.run.until_not_reached"));
    outcome_.failed = outcome_.failed || engine_.scripts().error_count() > 0;
    return std::move(outcome_);
  }

 private:
  bool stopped() const { return reached_ || outcome_.failed; }

  void fail(const std::string& problem) {
    outcome_.failed = true;
    outcome_.problem = frame_label(outcome_.frames) + problem;
  }

  void input() {
    if (next_input_ == plan_.input.size()) return;
    const RunInput& event = plan_.input[next_input_];
    if (event.frame != outcome_.frames) return;
    ScriptedInputSource& source = engine_.scripted_input();
    source.release_all();
    for (const Key key : event.keys) source.set_key(key, true);
    ++next_input_;
  }

  RubyResult evaluate(const std::string& expression, const char* option) {
    RubyResult result = engine_.scripts().eval(expression, kExpressionFile);
    if (!result.ok) fail(std::string(option) + ": " + compact_message(result.error, {}));
    return result;
  }

  void assertion() {
    if (!plan_.assertion) return;
    const RubyResult result = evaluate(*plan_.assertion, kAssertOption);
    if (result.ok && !mrb_test(result.value))
      fail(tr("project.run.assertion_failed", {{"expression", printable(*plan_.assertion)}}));
  }

  void until() {
    if (!plan_.until || outcome_.failed) return;
    const RubyResult result = evaluate(*plan_.until, kUntilOption);
    reached_ = result.ok && mrb_test(result.value);
  }

  void append_value(const std::string& value) {
    if (truncated_) return;
    const std::string line = frame_label(outcome_.frames) + value + "\n";
    if (outcome_.output.size() + line.size() <= kMostOutputBytes) {
      outcome_.output += line;
    } else if (!truncated_) {
      outcome_.output += tr("project.run.output_limit");
      truncated_ = true;
    }
  }

  void print_value() {
    if (!plan_.print) return;
    const RubyResult shown = evaluate("begin\n" + *plan_.print + "\nend.inspect", kPrintOption);
    if (!shown.ok) return;
    const bool is_text = mrb_string_p(shown.value);
    if (!is_text) return fail(tr("project.run.bad_inspect"));
    const std::string_view text(RSTRING_PTR(shown.value), static_cast<std::size_t>(RSTRING_LEN(shown.value)));
    append_value(short_value(text));
  }

  void step() {
    input();
    engine_.frame(config::kFixedDeltaSeconds);
    ++outcome_.frames;
    assertion();
    if (plan_.print && !outcome_.failed && outcome_.frames % plan_.every == 0) print_value();
    until();
  }

  Engine& engine_;
  const RunPlan& plan_;
  RunOutcome outcome_;
  std::size_t next_input_ = 0;
  bool reached_ = false;
  bool truncated_ = false;
};

}  // namespace

RunOutcome play_run(Engine& engine, const RunPlan& plan, long long frames) {
  return RunSession(engine, plan).run(frames);
}

}  // namespace redgem::tools
