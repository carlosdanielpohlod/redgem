#pragma once

// `redgem assets <game> --settings <asset> [field=value...]`: an asset's import settings (see
// core/import_settings.hpp). Without assignments it lists them; with some it sets those fields
// and writes the asset's sidecar file.

#include <filesystem>
#include <string>
#include <vector>

#include "project/tool_line.hpp"

namespace redgem::tools {

inline constexpr const char* kSettingsOption = "--settings";

// `asset` is a path inside the game ("models/tree.glb"); `assignments` are "field=value" words.
ToolResult asset_settings(const std::filesystem::path& game_dir, const std::string& asset,
                          const std::vector<std::string>& assignments);

}  // namespace redgem::tools
