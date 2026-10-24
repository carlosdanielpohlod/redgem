#include "script/script_budget.hpp"

#include <mruby.h>

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/coarse_clock.hpp"
#include "core/signal_stack.hpp"
#include "script/script_config.hpp"

namespace redgem {
namespace {

using std::chrono::milliseconds;

constexpr std::size_t kUsualDepth = 16;  // calls in calls, room made for them at once

// The one thread that looks at every limited interpreter of the program. It sleeps until the
// earliest deadline of the calls running; with none running, until one begins (begin_call rings
// it then, which costs a call nothing while the watchdog is awake).
class Watchdog {
 public:
  static Watchdog& instance() {
    // (Never destroyed: an interpreter may still be closing while the program's statics are.)
    static auto* watchdog = new Watchdog();
    return *watchdog;
  }

  void add(ScriptBudget* budget) {
    const std::lock_guard<std::mutex> hold(lock_);
    if (std::find(budgets_.begin(), budgets_.end(), budget) == budgets_.end()) budgets_.push_back(budget);
    if (!thread_.joinable()) thread_ = std::thread([this] { run(); });
    wake_locked();
  }

  // Once this returns, the watchdog does not touch the budget again.
  void remove(ScriptBudget* budget) {
    const std::lock_guard<std::mutex> hold(lock_);
    budgets_.erase(std::remove(budgets_.begin(), budgets_.end(), budget), budgets_.end());
  }

  // A call began somewhere: wakes the watchdog if it sleeps for want of calls.
  void call_began() {
    if (!idle_.load(std::memory_order_seq_cst)) return;
    const std::lock_guard<std::mutex> hold(lock_);
    wake_locked();
  }

 private:
  Watchdog() = default;

  void wake_locked() {
    idle_.store(false, std::memory_order_seq_cst);
    wake_.notify_all();
  }

  [[noreturn]] void run() {
    give_this_thread_a_signal_stack();
    std::unique_lock<std::mutex> hold(lock_);
    for (;;) {
      const std::int64_t next = look(coarse_milliseconds());
      if (next == ScriptBudget::kNever) {
        sleep_until_a_call(hold);
      } else {
        wake_.wait_for(hold, milliseconds(next - coarse_milliseconds()));
      }
    }
  }

  // Asks every interpreter whose call is overdue to stop it; when to look next.
  std::int64_t look(std::int64_t now) {
    std::int64_t next = ScriptBudget::kNever;
    for (ScriptBudget* budget : budgets_) next = std::min(next, look_at(*budget, now));
    return next;
  }

  // (A call that began since this look has a deadline no earlier than a whole limit from now.)
  static std::int64_t look_at(ScriptBudget& budget, std::int64_t now) {
    const std::int64_t deadline = budget.deadline();
    if (deadline > now) return std::min(deadline, now + budget.limit().count());
    budget.request_stop();
    return now + config::kScriptWatchRetryMs;  // (until the interpreter is back in Ruby to see it)
  }

  bool any_call_running() const {
    return std::any_of(budgets_.begin(), budgets_.end(),
                       [](const ScriptBudget* budget) { return budget->deadline() != ScriptBudget::kNever; });
  }

  // Says it is idle before looking once more: a call beginning at the same moment either is seen
  // here, or sees the watchdog idle and wakes it.
  void sleep_until_a_call(std::unique_lock<std::mutex>& hold) {
    idle_.store(true, std::memory_order_seq_cst);
    if (any_call_running()) {
      idle_.store(false, std::memory_order_seq_cst);
      return;
    }
    wake_.wait(hold, [this] { return !idle_.load(std::memory_order_seq_cst); });
  }

  std::mutex lock_;
  std::condition_variable wake_;
  std::vector<ScriptBudget*> budgets_;
  std::atomic<bool> idle_{false};
  std::thread thread_;
};

}  // namespace

ScriptBudget::~ScriptBudget() { Watchdog::instance().remove(this); }

void ScriptBudget::attach(mrb_state* mrb, RClass* timeout_class) {
  mrb_ = mrb;
  timeout_class_ = timeout_class;
  frames_.reserve(kUsualDepth);
}

void ScriptBudget::set_limit(Milliseconds limit) {
  limit_ms_.store(limit.count(), std::memory_order_relaxed);
  const bool limited = limit.count() > 0;
  if (limited) {
    Watchdog::instance().add(this);
  } else {
    Watchdog::instance().remove(this);
  }
}

void ScriptBudget::push(bool limited) {
  const std::int64_t now = coarse_milliseconds();
  if (!frames_.empty()) frames_.back().left_at = now;
  Frame frame;
  frame.limited = limited;
  frame.started = now;
  frames_.push_back(frame);
  publish(deadline_of(frame));
}

void ScriptBudget::pop() {
  frames_.pop_back();
  if (frames_.empty()) {
    publish(kNever);
    silence();
    return;
  }
  resume(frames_.back(), coarse_milliseconds());
}

// Back in a call from what ran inside it, which does not count: a call stopped for good is
// stopped again at its next instruction.
void ScriptBudget::resume(Frame& frame, std::int64_t now) {
  const std::int64_t away = now - frame.left_at;
  frame.started += away;
  frame.unwinding += away;
  publish(deadline_of(frame));
  if (frame.phase == Phase::Stopped) request_stop();
}

std::int64_t ScriptBudget::deadline_of(const Frame& frame) const {
  if (!counts(frame)) return kNever;
  switch (frame.phase) {
    case Phase::Running:
      return frame.started + limit().count();
    case Phase::Unwinding:
      return frame.unwinding + config::kScriptGraceMs;
    case Phase::Stopped:
      break;
  }
  return kNever;
}

// (A deadline to keep: the watchdog is woken if it sleeps for want of one.)
bool ScriptBudget::counts(const Frame& frame) const { return frame.limited && limit().count() > 0; }

void ScriptBudget::publish(std::int64_t deadline) {
  deadline_.store(deadline, std::memory_order_seq_cst);
  if (deadline != kNever) Watchdog::instance().call_began();
}

bool ScriptBudget::overdue(const Frame& frame, std::int64_t now) const {
  return frame.phase == Phase::Stopped || now >= deadline_of(frame);
}

void ScriptBudget::on_stop_request(mrb_state* mrb, const mrb_code* pc) {
  const std::int64_t now = coarse_milliseconds();
  if (frames_.empty() || !overdue(frames_.back(), now)) {
    silence();  // (not this call's, or not yet)
    return;
  }
  Frame& frame = frames_.back();
  step_up(frame, now);
  raise(mrb, pc, frame, now);
}

// Running: stopped once, with its grace to unwind. Unwinding past it: stopped for good.
void ScriptBudget::step_up(Frame& frame, std::int64_t now) {
  if (frame.phase == Phase::Running) {
    frame.phase = Phase::Unwinding;
    frame.unwinding = now;
    silence();
  } else {
    frame.phase = Phase::Stopped;
  }
  publish(deadline_of(frame));
}

void ScriptBudget::raise(mrb_state* mrb, const mrb_code* pc, const Frame& frame, std::int64_t now) {
  // The interpreter looks for the rescue of an error by where the instruction that raised it
  // ends, and the hook runs before the instruction: told the truth, the first instruction of a
  // rescue clause would count as inside its own begin, and be rescued by it again, forever.
  mrb->c->ci->pc = pc + 1;
  std::string message = "ran for " + std::to_string(now - frame.started) + " ms without returning, past the " +
                        std::to_string(limit().count()) + " ms a call into the scripts may take (" +
                        config::kScriptBudgetKey + " in game.json)";
  if (frame.phase == Phase::Stopped) message += "; it went on in a rescue or an ensure after it was stopped";
  mrb_raise(mrb, timeout_class_, message.c_str());
}

void ScriptBudget::request_stop() { set_hook(budget_hook); }

void ScriptBudget::silence() { set_hook(nullptr); }

// (mruby reads it atomically: see cmake/patch_mruby.cmake.)
void ScriptBudget::set_hook(Hook hook) { __atomic_store_n(&mrb_->code_fetch_hook, hook, __ATOMIC_RELAXED); }

void budget_hook(mrb_state* mrb, const mrb_irep*, const mrb_code* pc, mrb_value*) {
  static_cast<ScriptBudget*>(mrb->allocf_ud)->on_stop_request(mrb, pc);
}

}  // namespace redgem
