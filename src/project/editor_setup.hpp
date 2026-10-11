#pragma once

#include <vector>

#include "editor/editor_app.hpp"
#include "render/gpu_select.hpp"

namespace redgem {

// The ways of building a game the editor offers under Build Game: one for each system a game
// can be built for, this one's first.
std::vector<BuildChoice> build_choices();

// The editor as the program opens it: on the graphics card chosen, able to build games.
EditorOptions editor_options(GpuChoice gpu);

}  // namespace redgem
