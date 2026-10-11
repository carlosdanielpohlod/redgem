#pragma once

// The numbers of the program that runs a game: the window it opens.

namespace redgem::config {

// The window of a game that does not say (game.json's window_width and window_height).
inline constexpr int kWindowWidth = 1280;
inline constexpr int kWindowHeight = 720;
// What a game may ask for as a side of its window, in pixels (its game.json is not trusted to be sane).
inline constexpr int kMinWindowSide = 320;
inline constexpr int kMaxWindowSide = 16384;

}  // namespace redgem::config
