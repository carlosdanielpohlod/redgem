#pragma once

// `redgem shot <game> <out.png> [--at seconds] [--scene name] [--size WxH] [--camera name]`:
// plays a game to a moment, each frame one fixed step as a recording does, drawn off screen in a
// hidden window, and saves the picture of that frame. Small unless asked otherwise (512x288): a
// picture to look at what a level looks like costs little to read. What went wrong in the game's
// scripts on the way is said as `redgem run` says it.

#include <string>
#include <vector>

#include "project/tool_line.hpp"

namespace redgem::tools {

inline constexpr const char* kShotCommand = "shot";

ToolResult run_shot_command(const std::vector<std::string>& args);

}  // namespace redgem::tools
