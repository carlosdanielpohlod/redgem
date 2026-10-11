#include "testing/system_test.hpp"

#include <mruby.h>
#include <mruby/string.h>
#include <mruby/variable.h>

#include <memory>
#include <optional>
#include <string>
#include <variant>

#include "app/engine.hpp"
#include "core/exit_codes.hpp"
#include "core/log.hpp"
#include "testing/test_library.hpp"
#include "testing/test_saves.hpp"

namespace redgem::testing {
namespace {

// A system test that has not finished after this many frames fails.
constexpr long long kMaxFrames = 3600;
// Where SystemTest.define leaves the test for whoever runs it.
constexpr const char* kSystemTestGlobal = "$__redgem_system_test";
// What a system test answers to, and what its __tick says about how it is going.
constexpr const char* kTickMethod = "__tick";
constexpr const char* kFailureMethod = "failure";
constexpr const char* kPassed = "passed";
constexpr const char* kFailed = "failed";

enum class Status { Running, Passed, Failed };

// An engine as a test needs it: the same run every time, with the test at the controls.
EngineOptions engine_options(const SystemTestOptions& options, const ScriptedInputSource& input) {
  EngineOptions engine;
  engine.game_dir = options.game_dir;
  engine.headless = options.headless;
  engine.screenshot_dir = options.screenshot_dir;
  engine.input = &input;  // the devices are not read: a key touched by accident changes nothing
  engine.save_file = fresh_test_save_file(options.game_dir);
  engine.fixed_step = true;
  engine.show_fps = false;        // a number that changes from run to run has no place in a picture
  engine.capture_cursor = false;  // the pointer stays with whoever sits at the machine
  return engine;
}

// One system test playing one game, from the first frame to the verdict.
class SystemTestRun {
 public:
  explicit SystemTestRun(Engine& engine) : engine_(engine) {}

  // Reads the test's file. Returns what is wrong with it, if anything is.
  std::optional<std::string> load(const std::filesystem::path& file);
  // Plays until the test ends (or the game does). Returns the process exit code.
  int play();

 private:
  void tick();
  void heed(const std::string& said);
  void fail(const std::string& why);
  int verdict() const;
  RubyVm& vm() { return engine_.scripts().vm(); }

  Engine& engine_;
  mrb_value test_ = mrb_nil_value();  // (the global keeps it from Ruby's garbage collector)
  Status status_ = Status::Running;
};

std::optional<std::string> SystemTestRun::load(const std::filesystem::path& file) {
  const RubyResult loaded = engine_.scripts().load_file(file);
  if (!loaded.ok) return "cannot load system test: " + loaded.error;
  mrb_state* mrb = vm().state();
  test_ = mrb_gv_get(mrb, mrb_intern_cstr(mrb, kSystemTestGlobal));
  if (mrb_nil_p(test_)) return std::string("system test did not call SystemTest.define");
  return std::nullopt;
}

int SystemTestRun::play() {
  while (!engine_.finished()) {
    // The test moves first, so what it presses is what the game reads in the same frame.
    tick();
    if (status_ != Status::Running) break;
    engine_.frame(engine_.frame_seconds());
  }
  return verdict();
}

void SystemTestRun::fail(const std::string& why) {
  log::error(why);
  status_ = Status::Failed;
}

void SystemTestRun::tick() {
  if (engine_.script_context().time.frame >= kMaxFrames) {
    fail("system test timed out after " + std::to_string(kMaxFrames) + " frames");
    return;
  }
  const RubyResult result = vm().call(test_, kTickMethod);
  if (!result.ok) {
    fail("system test crashed: " + result.error);
  } else if (!mrb_symbol_p(result.value)) {
    fail("SystemTest#__tick must return a Symbol");
  } else {
    heed(mrb_sym_name(vm().state(), mrb_symbol(result.value)));
  }
}

void SystemTestRun::heed(const std::string& said) {
  if (said == kPassed) status_ = Status::Passed;
  if (said != kFailed) return;
  const RubyResult failure = vm().call(test_, kFailureMethod);
  const bool readable = failure.ok && mrb_string_p(failure.value);
  fail("system test failed: " + std::string(readable ? RSTRING_PTR(failure.value) : "unknown failure"));
}

int SystemTestRun::verdict() const {
  const int errors = engine_.scripts().error_count();
  if (status_ == Status::Passed && errors == 0) {
    log::info("system test passed");
    return kExitSuccess;
  }
  // (The window was closed, or the game quit, with the test still at it.)
  if (status_ == Status::Running) log::error("system test stopped before it ended");
  if (errors > 0) log::error(std::to_string(errors) + " script error(s)");
  return kExitFailure;
}

// The test's file, loaded into a game that is ready to play, and played.
int play_test(Engine& engine, ScriptedInputSource& input, const std::filesystem::path& test_file) {
  SystemTestRun run(engine);
  std::optional<std::string> error = load_test_library(engine.scripts(), input);
  if (!error) error = run.load(test_file);
  if (error) {
    log::error(*error);
    return kExitFailure;
  }
  return run.play();
}

}  // namespace

int run_system_test(const SystemTestOptions& options) {
  ScriptedInputSource input;  // what TestInput sets and the game reads; made first, so it outlives the engine
  auto created = Engine::create(engine_options(options, input));
  if (auto* error = std::get_if<std::string>(&created)) {
    log::error(*error);
    return kExitFailure;
  }
  return play_test(*std::get<std::unique_ptr<Engine>>(created), input, options.test_file);
}

}  // namespace redgem::testing
