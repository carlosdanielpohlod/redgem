#pragma once

#include <mruby.h>

#include "fake_clock.hpp"
#include "script/ruby_vm.hpp"

namespace redgem::test {

// An interpreter whose calls are timed on a test's clock, which its Ruby moves with `tick`: each
// tick, `step` seconds pass. One interpreter ticks at a time; the clock outlives the interpreter.
class ScriptTicks {
 public:
  ScriptTicks(RubyVm& vm, FakeClock& clock, double step) {
    current() = Tick{&clock, step};
    vm.budget_keeper().set_clock(&clock);
    mrb_define_method(vm.state(), vm.state()->kernel_module, "tick", tick, MRB_ARGS_NONE());
  }
  ~ScriptTicks() { current() = Tick{}; }
  ScriptTicks(const ScriptTicks&) = delete;
  ScriptTicks& operator=(const ScriptTicks&) = delete;

 private:
  struct Tick {
    FakeClock* clock = nullptr;
    double step = 0.0;
  };

  static Tick& current() {
    static Tick ticking;
    return ticking;
  }

  static mrb_value tick(mrb_state*, mrb_value) {
    current().clock->advance(current().step);
    return mrb_nil_value();
  }
};

}  // namespace redgem::test
