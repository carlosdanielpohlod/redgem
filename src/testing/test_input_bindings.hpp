#pragma once

#include <mruby.h>

#include "input/input.hpp"

namespace redgem::testing {

// The Ruby module tests press keys, move the mouse and type through.
inline constexpr const char* kTestInputModule = "TestInput";

// Defines TestInput in a Ruby VM: what it sets is what `source` reports from then on. The engine
// knows nothing of it, so the VM itself remembers which source that is (see driven_input).
// `source` has to outlive the VM.
void install_test_input(mrb_state* mrb, ScriptedInputSource& source);

// The source TestInput drives in this VM. Only for natives: it raises a Ruby error when the VM
// has none.
ScriptedInputSource& driven_input(mrb_state* mrb);

// Adds what two games tested together tell each other with (TestInput.leave_note, TestInput.note).
void install_test_notes(mrb_state* mrb, RClass* test_input_module);

}  // namespace redgem::testing
