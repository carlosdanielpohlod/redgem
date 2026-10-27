#pragma once

#include <string>
#include <vector>

namespace redgem::tools {
// `redgem mcp <game> [--read-only] [--allow tool,tool]`: a local stdio server.
int run_mcp_command(const std::vector<std::string>& args);
}  // namespace redgem::tools
