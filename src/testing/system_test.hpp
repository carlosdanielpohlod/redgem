#pragma once

#include <filesystem>

#include "render/gpu_select.hpp"

namespace redgem::testing {

struct SystemTestOptions {
  std::filesystem::path game_dir;
  std::filesystem::path test_file;       // a Ruby file that calls SystemTest.define
  std::filesystem::path screenshot_dir;  // where the test's t.screenshot pictures go
  bool headless = false;                 // no window (and so no pictures)
  GpuChoice gpu = GpuChoice::Auto;       // the graphics card the window opens on
  // A folder of this run's own: its save file is kept there, and so are the notes it and the
  // tests run with it leave each other. Empty: folders every run on this machine shares, which
  // two runs at the same time would get in each other's way in.
  std::filesystem::path work_dir;
};

// Plays a game with a system test at the controls: the real game, frame by frame, with the keys,
// mouse and gamepad the test says. Every run is the same run: each frame is one fixed step, the
// devices are not read, and the game starts from no saves (in a save file of the tests' own).
// Returns the process exit code: 0 when the test passed and no script failed.
int run_system_test(const SystemTestOptions& options);

}  // namespace redgem::testing
