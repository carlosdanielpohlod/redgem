#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

#include "scene/scene_desc.hpp"
#include "project/tool_line.hpp"

namespace redgem::tools {

inline constexpr std::size_t kDefaultDiffLines = 50;

// Describes changes to object state and the scene assets objects use, matched by persistent ID.
// `limit` bounds detail lines (0 means all). Counts always describe the complete comparison.
std::string scene_difference(const SceneDesc& before, const SceneDesc& after, const std::string& name,
                             std::size_t limit = kDefaultDiffLines);

ToolResult compare_scene_file(const SceneDesc& current, const std::string& name, const std::filesystem::path& baseline,
                              std::size_t limit);

}  // namespace redgem::tools
