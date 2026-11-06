#pragma once

#include <filesystem>
#include <string>
#include "editor/document/scene_document.hpp"

namespace redgem::prefab_files {
// A new file's stem: lowercase ASCII letters, digits, underscores and hyphens.
bool valid_name(const std::string& name);
// Writes prefabs/<name>.glb without replacing existing files or changing the document.
// The root keeps its world orientation and size, with its placement position reset to zero.
bool save(SceneDocument& document, int root, const Transform& placement, const std::filesystem::path& game,
          const std::string& name, std::string& error);
}  // namespace redgem::prefab_files
