#pragma once

// Searches that answer in text, a line for each thing found, best first (see asset_search.hpp):
//
//   redgem assets <game> [words...] [--kind model|texture|sound|script|prefab] [--tag name] [--limit N]
//   redgem templates [words...] [--limit N]

#include <string>
#include <vector>

#include "project/tool_line.hpp"

namespace redgem::tools {

inline constexpr const char* kAssetsCommand = "assets";
inline constexpr const char* kTemplatesCommand = "templates";

ToolResult run_assets_command(const std::vector<std::string>& args);
ToolResult run_templates_command(const std::vector<std::string>& args);

}  // namespace redgem::tools
