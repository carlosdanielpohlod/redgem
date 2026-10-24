#include "script/script_budget.hpp"

#include <mruby.h>

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "script/script_config.hpp"

namespace redgem {
namespace {

using std::chrono::duration_cast;
using std::chrono::milliseconds;

// The one thread that looks at every limited interpreter of the program, every
// kWatchInterval; it sleeps for as long as there are none.
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
    wake_.notify_all();
  }

  // Once this returns, the watchdog does not touch the budget nor its interpreter again.
  void remove(ScriptBudget* budget) {
    const std::lock_guard<std::mutex> hold(lock_);
    budgets_.erase(std::remove(budgets_.begin(), budgets_.end(), budget), budgets_.end());
  }

 private:
  Watchdog() = default;

  [[noreturn]] void run() {
    std::unique_lock<std::mutex> hold(lock_);
    for (;;) {
      wake_.wait(hold, [this] { return !budgets_.empty(); });
      wake_.wait_for(hold, milliseconds(config::kScriptWatchIntervalMs));
      const ScriptBudget::Clock::time_point now = ScriptBudget::Clock::now();
      for (ScriptBudget* budget : budgets_) budget->look(now);
    }
  }

  std::mutex lock_;
  std::condition_variable wake_;
  std::vector<ScriptBudget*> budgets_;
  std::thread thread_;
};

}  // namespace

void ScriptBudget::attach(mrb_state* mrb, RClass* timeout_class) {
  mrb_ = mrb;
  timeout_class_ = timeout_class;
}

void ScriptBudget::set_limit(milliseconds limit) {
  limit_ms_.store(limit.count());
  if (limit.count() > 0) {
    Watchdog::instance().add(this);
  } else {
    Watchdog::instance().remove(this);
  }
}

void ScriptBudget::begin_call() {
  if (depth_++ > 0) return;
  serial_.store(serial_.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
  calling_.store(true, std::memory_order_release);
}

void ScriptBudget::end_call() {
  if (--depth_ > 0) return;
  calling_.store(false, std::memory_order_release);
  // (What the engine does next with Ruby, describing the error, is a call of its own.)
  if (stopped_this_call()) set_hook(false);
}

bool ScriptBudget::stopped_this_call() const {
  return stopped_serial_.load(std::memory_order_acquire) == serial_.load(std::memory_order_relaxed);
}

void ScriptBudget::on_instruction(mrb_state* mrb, const mrb_code* pc) {
  if (!stopped_this_call()) {
    set_hook(false);  // (set for a call that ended just before it was: nothing to stop)
    return;
  }
  raise_stopped(mrb, pc);
}

bool ScriptBudget::same_call_as_seen(bool calling, std::uint64_t serial) const {
  return calling && serial == seen_serial_;
}

void ScriptBudget::look(Clock::time_point now) {
  const bool calling = calling_.load(std::memory_order_acquire);
  const std::uint64_t serial = serial_.load(std::memory_order_acquire);
  if (!same_call_as_seen(calling, serial)) {
    seen_serial_ = serial;
    seen_since_ = now;  // (it began at most one look before)
    return;
  }
  if (now - seen_since_ >= limit()) stop_call(serial, now - seen_since_);
}

// (Again at every look while the call runs: the hook may have been taken off by the end of the
// call before, at the same moment as it was set.)
void ScriptBudget::stop_call(std::uint64_t serial, Clock::duration ran) {
  if (stopped_serial_.load() != serial) stopped_after_ms_.store(duration_cast<milliseconds>(ran).count());
  stopped_serial_.store(serial, std::memory_order_release);
  set_hook(true);
}

void ScriptBudget::set_hook(bool on) {
  using Hook = decltype(mrb_->code_fetch_hook);
  const Hook hook = on ? budget_hook : nullptr;
  __atomic_store_n(&mrb_->code_fetch_hook, hook, __ATOMIC_RELEASE);
}

void ScriptBudget::raise_stopped(mrb_state* mrb, const mrb_code* pc) {
  // The interpreter looks for the rescue of an error by where the instruction that raised it
  // ends, and the hook runs before the instruction: told the truth, the first instruction of a
  // rescue clause would count as inside its own begin, and be rescued by it again, forever.
  mrb->c->ci->pc = pc + 1;
  const std::string message = "ran for more than " + std::to_string(stopped_after_ms_.load()) +
                              " ms without returning, past the " + std::to_string(limit().count()) +
                              " ms a call into the scripts may take (" + config::kScriptBudgetKey + " in game.json)";
  mrb_raise(mrb, timeout_class_, message.c_str());
}

void budget_hook(mrb_state* mrb, const mrb_irep*, const mrb_code* pc, mrb_value*) {
  static_cast<ScriptBudget*>(mrb->allocf_ud)->on_instruction(mrb, pc);
}

}  // namespace redgem
