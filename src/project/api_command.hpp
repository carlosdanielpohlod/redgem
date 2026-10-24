#pragma once

// `redgem api [Class] [--search word] [--json] [--markdown]`: the scripting API, short. Alone it
// is an index, a line per class; with a class, that class's methods; with --search, the methods
// that have to do with a word. --json says the same as data, for tools; --markdown writes the
// manual's page (docs/scripting/api.md).

#include <string>
#include <vector>

#include "project/tool_line.hpp"

namespace redgem::tools {

inline constexpr const char* kApiCommand = "api";

ToolResult run_api_command(const std::vector<std::string>& args);

}  // namespace redgem::tools
