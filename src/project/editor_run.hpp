#pragma once

#include <filesystem>

#include "render/gpu_select.hpp"

namespace redgem {

// The local tools an editor opens for the programs of its developer.
struct EditorTooling {
  bool bridge = false;     // --editor-bridge: the authenticated local bridge (project/editor_bridge.hpp)
  bool ruby_eval = false;  // --ruby-eval: the bridge, answering game_eval too (project/ruby_eval.hpp)
};

// The editor, from the moment it is asked for until it is left: on `game`, or, when none is
// given (and for File > New / Open Game), on whatever is picked in the project hub. Every window
// it opens is on the card `gpu` says. Returns the program's exit code.
int run_editor(const std::filesystem::path& game, GpuChoice gpu, EditorTooling tooling = {});

}  // namespace redgem
