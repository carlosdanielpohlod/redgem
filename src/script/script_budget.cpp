#include "script/script_budget.hpp"

#include <mruby.h>

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/coarse_clock.hpp"
#include "core/engine_thread.hpp"
#include "script/script_config.hpp"

namespace redgem {
namespace {

using std::chrono::milliseconds;

constexpr std::size_t kUsualDepth = 16;  // calls in calls, room made for them at once

std::atomic<std::uint64_t> watchdog_wakes{0};

// The one thread that looks at every limited interpreter of the program. It sleeps until the
// earliest deadline of the calls running, and with none running, until one begins: a deadline
// earlier than the one it sleeps until wakes it (deadline_set), which costs a call nothing while
// the watchdog already waits for an earlier one. After a look that finds no call running it waits
// one more limit before it sleeps for good, so that a game calling its scripts every frame wakes
// it about once a limit, not once a call.
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
    if (!thread_.joinable()) thread_ = std::thread([this] { run_engine_thread([this] { run(); }); });
    wake_locked(ScriptBudget::kNever - 1);
  }

  // Once this returns, the watchdog does not touch the budget again.
  void remove(ScriptBudget* budget) {
    const std::lock_guard<std::mutex> hold(lock_);
    budgets_.erase(std::remove(budgets_.begin(), budgets_.end(), budget), budgets_.end());
  }

  // A call has a deadline: wakes the watchdog if it would look later than that.
  void deadline_set(std::int64_t deadline) {
    if (deadline >= planned_.load(std::memory_order_seq_cst)) return;
    const std::lock_guard<std::mutex> hold(lock_);
    wake_locked(deadline);
  }

 private:
  Watchdog() = default;

  void wake_locked(std::int64_t deadline) {
    planned_.store(std::min(deadline, planned_.load(std::memory_order_seq_cst)), std::memory_order_seq_cst);
    wake_.notify_all();
  }

  [[noreturn]] void run() {
    std::unique_lock<std::mutex> hold(lock_);
    for (;;) {
      watchdog_wakes.fetch_add(1, std::memory_order_relaxed);
      const std::int64_t now = coarse_milliseconds();
      sleep_until(hold, linger(look(now), now));
    }
  }

  // Asks every interpreter whose call is overdue to stop it; when to look next.
  std::int64_t look(std::int64_t now) {
    std::int64_t next = ScriptBudget::kNever;
    for (ScriptBudget* budget : budgets_) next = std::min(next, look_at(*budget, now));
    return next;
  }

  static std::int64_t look_at(ScriptBudget& budget, std::int64_t now) {
    const std::int64_t deadline = budget.deadline();
    if (deadline > now) return deadline;  // (kNever for a budget with no call running)
    budget.request_stop();
    return now + config::kScriptWatchRetryMs;  // (until the interpreter is back in Ruby to see it)
  }

  // The first look that finds no call running waits one more limit; the next sleeps for good.
  std::int64_t linger(std::int64_t next, std::int64_t now) {
    const bool found_none = next == ScriptBudget::kNever;
    const bool first_of_none = found_none && !found_none_before_;
    found_none_before_ = found_none;
    // (With no interpreter limited any more, the shortest limit is "never".)
    return first_of_none ? milliseconds_after(now, shortest_limit()) : next;
  }

  std::int64_t shortest_limit() const {
    std::int64_t shortest = ScriptBudget::kNever;
    for (const ScriptBudget* budget : budgets_) shortest = std::min<std::int64_t>(shortest, budget->limit().count());
    return shortest;
  }

  std::int64_t earliest_deadline() const {
    std::int64_t earliest = ScriptBudget::kNever;
    for (const ScriptBudget* budget : budgets_) earliest = std::min(earliest, budget->deadline());
    return earliest;
  }

  // Says until when it sleeps before looking once more: a call beginning at the same moment either
  // is seen here, or sees the plan and wakes it.
  void sleep_until(std::unique_lock<std::mutex>& hold, std::int64_t next) {
    planned_.store(next, std::memory_order_seq_cst);
    if (earliest_deadline() < next) return;
    const auto woken_early = [this, next] { return planned_.load(std::memory_order_seq_cst) < next; };
    if (next == ScriptBudget::kNever) {
      wake_.wait(hold, woken_early);
    } else {
      wake_.wait_for(hold, milliseconds(next - coarse_milliseconds()), woken_early);
    }
  }

  std::mutex lock_;
  std::condition_variable wake_;
  std::vector<ScriptBudget*> budgets_;
  std::atomic<std::int64_t> planned_{ScriptBudget::kNever};  // when it looks next
  bool found_none_before_ = false;
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
  // (Set here, on the interpreter's own thread, and never from another.)
  mrb_->code_fetch_hook = limited ? budget_hook : nullptr;
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

// Back in a call from what ran inside it, which does not count (up to a point): a call that is
// overdue now, or was stopped for good, is stopped at its next instruction.
void ScriptBudget::resume(Frame& frame, std::int64_t now) {
  const std::int64_t away = forgivable(frame, now - frame.left_at);
  frame.forgiven += away;
  frame.started += away;
  frame.unwinding += away;
  publish(deadline_of(frame));
  // (Asked here, not left to the watchdog, which may only ever have looked while the engine
  // worked: a loop that does nothing but spawn is overdue as it comes back from a spawn.)
  if (overdue(frame, now)) request_stop();
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

std::int64_t ScriptBudget::forgivable(const Frame& frame, std::int64_t away) const {
  const std::int64_t most = config::kScriptForgivenBudgets * limit().count();
  return std::max<std::int64_t>(0, std::min(away, most - frame.forgiven));
}

bool ScriptBudget::counts(const Frame& frame) const { return frame.limited && limit().count() > 0; }

// (A deadline to keep: the watchdog is woken if it would look later than that.)
void ScriptBudget::publish(std::int64_t deadline) {
  deadline_.store(deadline, std::memory_order_seq_cst);
  if (deadline != kNever) Watchdog::instance().deadline_set(deadline);
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

void budget_hook(mrb_state* mrb, const mrb_irep*, const mrb_code* pc, mrb_value*) {
  auto* budget = static_cast<ScriptBudget*>(mrb->allocf_ud);
  if (budget->stop_requested()) budget->on_stop_request(mrb, pc);
}

std::uint64_t script_watchdog_wakes() { return watchdog_wakes.load(std::memory_order_relaxed); }

}  // namespace redgem
