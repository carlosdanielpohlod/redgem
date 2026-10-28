#pragma once

#include <filesystem>

#include "render/gpu_select.hpp"

namespace redgem {

// The editor, from the moment it is asked for until it is left: on `game`, or, when none is
// given (and for File > New / Open Game), on whatever is picked in the project hub. Every window
// it opens is on the card `gpu` says. Returns the program's exit code.
int run_editor(const std::filesystem::path& game, GpuChoice gpu, bool bridge = false);

}  // namespace redgem
