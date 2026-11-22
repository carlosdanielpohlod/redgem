#pragma once

// MCP's import_settings tool: how the engine takes in one asset of the game (see
// core/import_settings.hpp and docs/import-settings.md).
//
//   settings   the asset's fields, their values and their choices (the defaults without a sidecar)
//   set        `fields` {"scale": 0.01, "up_axis": "z"} into its sidecar file, all or none (null
//              puts a field back to its default)
//   apply      a model: imports it again wherever the editor's open scene holds it (only a running
//              editor does that; see editor_live_tools.cpp)

#include <filesystem>
#include <string>

#include "project/mcp_protocol.hpp"

namespace redgem::tools::mcp {

struct Options;

// settings and set, on the game's files (the same with an editor connected or not); apply refuses
// here, saying it needs the editor.
Json import_settings_tool(const Options& options, const Json& args);

// The same settings and set, for a game's folder.
Json import_settings_files(const std::filesystem::path& game, const Json& args);

// The model file apply imports again, as the editor remembers it on each copy (its path inside
// models/: "trees/oak.glb"); throws for an asset that is no model of models/.
std::string model_to_reimport(const std::filesystem::path& game, const Json& args);

// Whether an action only reads (what --read-only keeps).
bool reads_import_settings(const std::string& action);

}  // namespace redgem::tools::mcp
