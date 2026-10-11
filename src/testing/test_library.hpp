#pragma once

#include <optional>
#include <string>

#include "input/input.hpp"
#include "script/script_host.hpp"

namespace redgem::testing {

// Gives a game's Ruby what tests are written with, which the engine itself does not have:
// TestInput (driving `input`), the assertions and test cases of RedGemTest, and the SystemTest
// DSL. Call it after the host is made (the engine's own prelude is loaded by then) and before
// loading a test. `input` has to outlive the host. Returns what went wrong, if anything did.
std::optional<std::string> load_test_library(ScriptHost& host, ScriptedInputSource& input);

}  // namespace redgem::testing
