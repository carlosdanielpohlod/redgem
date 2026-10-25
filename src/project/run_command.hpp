#pragma once

// `redgem run <game> [--scene name] [--seconds N | --frames N]`: plays a game with
// no window for a while (5 seconds unless said, each frame one fixed step) and says only what
// went wrong, a line each with the place in the game's scripts, then one line of summary. It ends
// with an error when a script failed.

#include <string>
#include <vector>

#include "project/tool_line.hpp"

namespace redgem::tools {

inline constexpr const char* kRunCommand = "run";

ToolResult run_run_command(const std::vector<std::string>& args);

}  // namespace redgem::tools
