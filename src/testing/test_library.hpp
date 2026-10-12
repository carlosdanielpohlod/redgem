#pragma once

#include <optional>
#include <string>

#include "input/input.hpp"
#include "script/script_host.hpp"
#include "testing/frame_pictures.hpp"

namespace redgem::testing {

// Gives a game's Ruby what tests are written with, which the engine itself does not have:
// TestInput (driving `input`), the assertions and test cases of RedGemTest, and the SystemTest
// DSL. Call it after the host is made (the engine's own prelude is loaded by then) and before
// loading a test. With `pictures`, the game's Ruby also gets TestScreen, which takes them (what
// t.screenshot asks; tests that play no game have none to take). `input` and `pictures` have to
// outlive the host. Returns what went wrong, if anything did.
std::optional<std::string> load_test_library(ScriptHost& host, ScriptedInputSource& input,
                                             FramePictures* pictures = nullptr);

}  // namespace redgem::testing
