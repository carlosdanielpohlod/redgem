#pragma once

#include <vector>

#include "editor/editor_app.hpp"
#include "render/gpu_select.hpp"

namespace redgem {

// The ways of building a game the editor offers under Build Game: one for each system a game
// can be built for, this one's first.
std::vector<BuildChoice> build_choices();

// What `redgem check` finds in a game, for the editor's File > Check Game.
GameCheck check_for_editor(const std::filesystem::path& game_dir);

// The editor as the program opens it: on the graphics card chosen, able to build and check games.
EditorOptions editor_options(GpuChoice gpu);

}  // namespace redgem
