#include "project/scene_lookup.hpp"

#include <algorithm>
#include <vector>

#include "core/translations.hpp"
#include "project/closest_words.hpp"
#include "project/scene_text.hpp"

namespace redgem::tools {
namespace {

// How many names a message about a missing object suggests.
constexpr std::size_t kMostSuggestions = 5;
constexpr char kPathSeparator = '/';

bool marked_as_id(const std::string& reference) { return !reference.empty() && reference.front() == kIdMark; }

bool is_path(const std::string& reference) { return reference.find(kPathSeparator) != std::string::npos; }

// The objects whose name (or, for a path, whose path) is `reference`.
std::vector<int> matches(const SceneDesc& scene, const std::string& reference) {
  std::vector<int> found;
  const bool by_path = is_path(reference);
  for (int index = 0; index < static_cast<int>(scene.nodes.size()); ++index) {
    const std::string name = by_path ? object_path(scene, index) : scene.nodes[index].name;
    if (name == reference) found.push_back(index);
  }
  return found;
}

std::vector<std::string> every_name(const SceneDesc& scene) {
  std::vector<std::string> names;
  for (const NodeDesc& node : scene.nodes) {
    if (std::find(names.begin(), names.end(), node.name) == names.end()) names.push_back(node.name);
  }
  return names;
}

FoundObject not_found(const SceneDesc& scene, const std::string& reference) {
  const std::vector<std::string> near = closest_words(reference, every_name(scene), kMostSuggestions);
  if (near.empty()) return {kNoIndex, tr("project.scene.no_object", {{"object", reference}})};
  return {kNoIndex, tr("project.scene.no_object_near", {{"object", reference}, {"names", joined(near)}})};
}

FoundObject ambiguous(const SceneDesc& scene, const std::string& reference, const std::vector<int>& found) {
  std::string list;
  for (const int index : found) list += "\n  " + object_id(scene, index) + " " + object_path(scene, index);
  return {kNoIndex,
          tr("project.scene.ambiguous", {{"object", reference}, {"count", std::to_string(found.size())}}) + list, true};
}

FoundObject by_id(const SceneDesc& scene, const std::string& reference) {
  for (int index = 0; index < static_cast<int>(scene.nodes.size()); ++index) {
    if (object_id(scene, index) == reference) return {index, {}};
  }
  return {kNoIndex, tr("project.scene.no_id", {{"id", reference}})};
}

}  // namespace

FoundObject find_object(const SceneDesc& scene, const std::string& reference) {
  if (marked_as_id(reference)) return by_id(scene, reference);
  const std::vector<int> found = matches(scene, reference);
  if (found.empty()) return not_found(scene, reference);
  if (found.size() > 1) return ambiguous(scene, reference, found);
  return {found.front(), {}};
}

}  // namespace redgem::tools
