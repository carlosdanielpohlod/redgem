#pragma once

#include <mruby.h>

#include <atomic>
#include <chrono>
#include <cstdint>

namespace redgem {

// How long one call from the engine into the scripts may run before it is stopped: a script that
// never returns (`loop {}`, `while true; end`) must not freeze the game, the editor or a room of
// a server. A call that runs past it is ended with a ScriptTimeout, which the engine treats as
// any other error (the behaviour it came from is disabled), and the game goes on.
//
// A call costs the interpreter nothing for it: a watchdog thread (one for the whole program)
// looks at every limited interpreter a few times per budget, and only once it finds the same call
// still running past its limit does it give that interpreter its code fetch hook, the function
// mruby calls before each instruction, which raises. (Setting the hook from another thread is
// what mruby's hook is good for: the interpreter reads it before every instruction, and a
// pointer is stored whole.) Time spent inside the engine's own functions counts too, but a
// function of the engine is never stopped halfway: the call ends at its next instruction of Ruby.
class ScriptBudget {
 public:
  using Clock = std::chrono::steady_clock;

  ScriptBudget() = default;
  ~ScriptBudget() { set_limit(std::chrono::milliseconds(0)); }
  ScriptBudget(const ScriptBudget&) = delete;
  ScriptBudget& operator=(const ScriptBudget&) = delete;

  // The interpreter it watches, and the class of the error a call that ran too long ends with.
  // Before any limit is set.
  void attach(mrb_state* mrb, RClass* timeout_class);
  // No limit (zero) until one is set. Once the limit is set back to zero, the watchdog no longer
  // looks at the interpreter (which can then be closed).
  void set_limit(std::chrono::milliseconds limit);
  std::chrono::milliseconds limit() const { return std::chrono::milliseconds(limit_ms_.load()); }

  // A call from the engine begins and ends. Calls made from inside one (an engine function that
  // calls back into Ruby) are part of it: only the outermost is a call of its own.
  void begin_call();
  void end_call();

  // The hook: raises while the call running is one the watchdog stopped. Every instruction raises
  // again, so a script that rescues the error cannot keep on running.
  void on_instruction(mrb_state* mrb, const mrb_code* pc);

  // The watchdog's look at the interpreter, at `now` (on its thread).
  void look(Clock::time_point now);

 private:
  bool stopped_this_call() const;
  // Whether the call the watchdog sees is the one it saw last time.
  bool same_call_as_seen(bool calling, std::uint64_t serial) const;
  void stop_call(std::uint64_t serial, Clock::duration ran);
  void set_hook(bool on);
  void raise_stopped(mrb_state* mrb, const mrb_code* pc);

  mrb_state* mrb_ = nullptr;
  RClass* timeout_class_ = nullptr;
  std::atomic<long long> limit_ms_{0};
  int depth_ = 0;  // of calls in calls, on the interpreter's thread
  // Which outermost call it is, counted from the first, and whether one is running.
  std::atomic<std::uint64_t> serial_{0};
  std::atomic<bool> calling_{false};
  // The call the watchdog stopped, and how long it had run then.
  std::atomic<std::uint64_t> stopped_serial_{0};
  std::atomic<long long> stopped_after_ms_{0};
  // What the watchdog saw last (on its thread alone).
  std::uint64_t seen_serial_ = 0;
  Clock::time_point seen_since_;
};

// mruby's code fetch hook, for an interpreter whose allocator data is its ScriptBudget.
void budget_hook(mrb_state* mrb, const mrb_irep* irep, const mrb_code* pc, mrb_value* regs);

}  // namespace redgem
