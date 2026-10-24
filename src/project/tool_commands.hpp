#pragma once

// The developer tools of redgem's command line that answer in text and end (see tool_line.hpp),
// told apart from the rest of the line by their first word: `redgem scene ...`, `redgem run ...`,
// `redgem shot ...`, `redgem check ...`.

#include <optional>
#include <string>
#include <vector>

#include "project/tool_line.hpp"

namespace redgem::tools {

// What the tool the first word names answers; nothing when the first word names no tool (the
// line is then one of the editor's, read by parse_cli).
std::optional<ToolResult> run_tool_command(const std::vector<std::string>& args);

}  // namespace redgem::tools
