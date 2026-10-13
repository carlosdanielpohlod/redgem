#pragma once

// What whoever opens the editor decides for it.

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "render/gpu_select.hpp"

namespace redgem {

// How long the frame about to be drawn counts for, in seconds; asked once a frame.
using FrameClock = std::function<float()>;

// One way of making a standalone game, as Build Game offers it. The editor only knows that such
// ways exist: whoever opens it says which, and does the building.
struct BuildChoice {
  std::string name;   // the system built for ("linux"): how the build is spoken of, and its folder named
  std::string label;  // what the menu shows
  // Whether it is for the system the editor runs on (the build then needs no system in its folder's name).
  bool for_this_system = false;
  // Builds the game in `game_dir` into `out_dir`. The game's executable (or app), or nullopt
  // with `error` saying why not.
  std::function<std::optional<std::filesystem::path>(const std::filesystem::path& game_dir,
                                                     const std::filesystem::path& out_dir, std::string& error)>
      build;
};

struct EditorOptions {
  // How long each frame counts for, for whoever drives the editor at a pace of its own. Empty:
  // the time the window measured between its last two frames.
  FrameClock clock;
  // The graphics card the window opens on, where the machine has two.
  GpuChoice gpu = GpuChoice::Auto;
  // The ways of building a game, in the order Build Game lists them. None: the editor builds nothing.
  std::vector<BuildChoice> builds;
};

}  // namespace redgem
