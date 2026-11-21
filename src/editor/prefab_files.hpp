#pragma once

#include <filesystem>
#include <string>
#include "editor/document/scene_document.hpp"
#include "scene/prefabs.hpp"

namespace redgem::prefab_files {
// A new file's stem: lowercase ASCII letters, digits, underscores and hyphens.
bool valid_name(const std::string& name);
// The game a prefab is written into, with the budgets its scenes expand prefabs within.
struct Game {
  std::filesystem::path folder;
  prefabs::Limits limits;
};
// Writes prefabs/<name>.glb without replacing existing files or changing the document.
// The root keeps its world orientation and size, with its placement position reset to zero.
bool save(SceneDocument& document, int root, const Transform& placement, const Game& game, const std::string& name,
          std::string& error);
}  // namespace redgem::prefab_files
