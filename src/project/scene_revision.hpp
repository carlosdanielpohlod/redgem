#pragma once

// A scene compared with the same scene as Git keeps it at a revision (`redgem scene diff <game>
// --against HEAD~1`): the file at that revision is read with `git show <revision>:<file>` and
// compared, by persistent #id, as an earlier save would be (scene_diff.hpp).

#include <cstddef>
#include <filesystem>
#include <string>

#include "project/tool_line.hpp"
#include "scene/scene_desc.hpp"

namespace redgem::tools {

// The longest revision taken: far more than any branch, tag or hash with ~ and ^ needs.
inline constexpr std::size_t kMostRevisionBytes = 256;

// Whether `against` names an earlier save (a .glb file) rather than a Git revision.
bool names_scene_file(const std::string& against);

// Whether `revision` can be handed to Git as one: a branch, tag or hash, with ~N, ^N or @{N}.
// Nothing Git would read as an option (a leading '-'), no range (".."), no path (':'), no
// spaces or quotes, and nothing that ends as a scene file does (.glb).
bool valid_revision(const std::string& revision);

// Compares `current` (the scene `name`, saved at `scene_file`) with that file at `revision`.
// Fails when the file is in no Git repository, when Git has no such revision or no such file
// at it, or when what it held is no scene.
ToolResult compare_scene_revision(const SceneDesc& current, const std::string& name,
                                  const std::filesystem::path& scene_file, const std::string& revision,
                                  std::size_t limit);

}  // namespace redgem::tools
