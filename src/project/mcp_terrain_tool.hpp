#pragma once

// MCP's `terrain` tool: a terrain's ground layers (terrains/<name>.json, see
// scene/ground/terrain_layers_file.hpp) read and written, and painted by their rules, as files.
//
//   layers       the game's layers files; one of them by `name`, or the one an `object` names
//   set_layers   writes `content` as terrains/<name>.json, checked as the game reads it (replacing
//                one needs confirm=true)
//   paint        paints the `object`'s layers by the rules of its file and writes the weights
//                beside it (redgem scene paint)

#include "project/mcp_tools.hpp"

namespace redgem::tools::mcp {

Json terrain_tool(const Options& options, const Json& args);

// The actions that only read, which --read-only keeps.
bool reads_terrain(const std::string& action);

}  // namespace redgem::tools::mcp
