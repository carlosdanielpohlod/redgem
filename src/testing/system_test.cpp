#include "testing/system_test.hpp"

#include <mruby.h>
#include <mruby/string.h>
#include <mruby/variable.h>

#include <chrono>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <variant>

#include "app/engine.hpp"
#include "core/exit_codes.hpp"
#include "core/log.hpp"
#include "core/translations.hpp"
#include "testing/frame_pictures.hpp"
#include "testing/test_library.hpp"
#include "testing/test_notes.hpp"
#include "testing/test_saves.hpp"
#include "testing/test_stage.hpp"

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
constexpr const char* kWaiting = "waiting";  // a frame of a wait in real seconds (SystemTest#wait_for)

// The frames at the game's pace (`step` seconds each) that `seconds` of waits in real seconds take.
long long frames_in(int seconds, float step) {
  return std::llround(static_cast<double>(seconds) / static_cast<double>(step));
}

enum class Status { Running, Passed, Failed };

// The files the run plays with: fresh ones, or copies of those of the run it goes on from.
RunFiles run_files(const SystemTestOptions& options) {
  if (options.after.empty()) return fresh_run_files(options.game_dir, options.work_dir, options.test_file);
  return copied_run_files(options.game_dir, options.work_dir, options.test_file, options.after);
}

// An engine as a test needs it: the same run every time, with the test at the controls.
EngineOptions engine_options(const SystemTestOptions& options, const RunFiles& files, ScriptedInputSource& input,
                             Stage& stage) {
  EngineOptions engine;
  engine.game_dir = options.game_dir;
  engine.display = stage.display();
  engine.display_kept_in = stage.display_kept_in();
  engine.presenter = stage.presenter();
  engine.input = &input;  // the devices are not read: a key touched by accident changes nothing
  engine.save_file = files.save;
  engine.bindings_file = files.bindings;
  engine.fixed_step = true;
  engine.audio = stage.audio();
  return engine;
}

// One system test playing one game, from the first frame to the verdict.
class SystemTestRun {
 public:
  SystemTestRun(Engine& engine, FramePictures& pictures, int wait_limit_seconds)
      : engine_(engine), pictures_(pictures), max_waited_frames_(frames_in(wait_limit_seconds, engine.fixed_delta())) {}

  // Reads the test's file. Returns what is wrong with it, if anything is.
  std::optional<std::string> load(const std::filesystem::path& file);
  // Plays until the test ends (or the game does). Returns the process exit code.
  int play();

 private:
  void tick();
  bool timed_out();
  void heed(const std::string& said);
  void wait_a_frame();
  void fail(const std::string& why);
  int verdict() const;
  RubyVm& vm() { return engine_.scripts().vm(); }

  Engine& engine_;
  FramePictures& pictures_;
  mrb_value test_ = mrb_nil_value();  // (the global keeps it from Ruby's garbage collector)
  Status status_ = Status::Running;
  long long max_waited_frames_;  // frames of waits in real seconds the test may spend in all
  long long waited_frames_ = 0;  // frames of waits in real seconds, which the limit does not count
  std::chrono::steady_clock::time_point last_waited_;  // when the last of them started
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
    engine_.frame(engine_.presenter().frame_seconds());
    pictures_.forget_pending();  // (whatever the frame's picture was not taken of is not coming)
  }
  return verdict();
}

// A frame of a wait in real seconds goes at the game's pace, for the other program to get on: it
// starts no sooner than a fixed step after the one before (a window that keeps the pace itself
// makes no wait longer).
void SystemTestRun::wait_a_frame() {
  ++waited_frames_;
  const auto step = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
      std::chrono::duration<float>(engine_.fixed_delta()));
  std::this_thread::sleep_until(last_waited_ + step);
  last_waited_ = std::chrono::steady_clock::now();
}

void SystemTestRun::fail(const std::string& why) {
  log::error(why);
  status_ = Status::Failed;
}

// Whether the test ran out of frames, its own or its waits in real seconds (it fails then).
bool SystemTestRun::timed_out() {
  if (engine_.script_context().time.frame - waited_frames_ >= kMaxFrames) {
    fail("system test timed out after " + std::to_string(kMaxFrames) + " frames");
    return true;
  }
  if (waited_frames_ >= max_waited_frames_) {
    fail("system test timed out after " + std::to_string(max_waited_frames_) +
         " frames of waits in real seconds (run alone, --wait-limit raises it)");
    return true;
  }
  return false;
}

void SystemTestRun::tick() {
  if (timed_out()) return;
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
  if (said == kWaiting) wait_a_frame();
  if (said != kFailed) return;
  const RubyResult failure = vm().call(test_, kFailureMethod);
  const bool readable = failure.ok && mrb_string_p(failure.value);
  fail("system test failed: " + std::string(readable ? RSTRING_PTR(failure.value) : "unknown failure"));
}

int SystemTestRun::verdict() const {
  const int errors = engine_.scripts().error_count();
  const bool passed = status_ == Status::Passed && errors == 0;
  if (passed) {
    log::info("system test passed");
    return kExitSuccess;
  }
  // (The window was closed, or the game quit, with the test still at it.)
  if (status_ == Status::Running) log::error("system test stopped before it ended");
  if (errors > 0) log::error(std::to_string(errors) + " script error(s)");
  return kExitFailure;
}

// The test's file, loaded into a game that is ready to play, and played.
int play_test(Engine& engine, ScriptedInputSource& input, FramePictures& pictures,
              const SystemTestOptions& options) {
  SystemTestRun run(engine, pictures, options.wait_limit_seconds);
  std::optional<std::string> error = load_test_library(engine.scripts(), input, &pictures);
  if (!error) error = run.load(options.test_file);
  if (error) {
    log::error(*error);
    return kExitFailure;
  }
  return run.play();
}

// Whether a run would go on from a test never run in its folder: a misspelt test, or a first run
// made elsewhere (it would start from nothing, and fail far from the cause).
bool goes_on_from_nothing(const SystemTestOptions& options) {
  return !options.after.empty() && !ran_in(options.work_dir, options.after);
}

// The test played on a game that starts with `files`.
int play_with(const SystemTestOptions& options, const RunFiles& files) {
  // Made before the engine, so they outlive it: what TestInput sets and the game reads, the
  // pictures the test asks for, and where the game is shown.
  ScriptedInputSource input;
  FramePictures pictures(options.screenshot_dir, options.game_dir);
  Stage stage(options, files.settings, pictures);
  if (!options.work_dir.empty()) keep_notes_in(notes_folder_in(options.work_dir));
  auto created = Engine::create(engine_options(options, files, input, stage));
  if (auto* error = std::get_if<std::string>(&created)) {
    log::error(*error);
    return kExitFailure;
  }
  return play_test(*std::get<std::unique_ptr<Engine>>(created), input, pictures, options);
}

}  // namespace

int run_system_test(const SystemTestOptions& options) {
  if (goes_on_from_nothing(options)) {
    log::error(tr("testing.run.never_ran", {{"test", options.after.string()}, {"folder", options.work_dir.string()}}));
    return kExitFailure;
  }
  const RunFiles files = run_files(options);
  if (!options.work_dir.empty()) mark_run(options.work_dir, options.test_file);
  return play_with(options, files);
}

}  // namespace redgem::testing
