#pragma once

// The numbers of the program that runs a game: the window it opens.

#include "render/render_config.hpp"

namespace redgem::config {

// The window of a game that does not say (game.json's window_width and window_height).
inline constexpr int kWindowWidth = 1280;
inline constexpr int kWindowHeight = 720;
// (What a side of the window may be: render_config.hpp.)

}  // namespace redgem::config
