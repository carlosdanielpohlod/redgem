#include "scene/scene_ids.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <random>
#include <set>

#include "core/safe_path.hpp"

namespace redgem {
namespace {

constexpr std::size_t kMaxSceneIdBytes = 64;
constexpr int kSceneIdHexDigits = 16;

bool id_character(char c) {
  const bool letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
  const bool digit = c >= '0' && c <= '9';
  return letter || digit || c == '-' || c == '_';
}

std::set<std::string> keep_unique_ids(SceneDesc& scene, std::vector<std::string>& warnings) {
  std::set<std::string> used;
  for (NodeDesc& node : scene.nodes) {
    if (node.stable_id.empty()) continue;
    const bool valid = valid_scene_id(node.stable_id) && used.insert(node.stable_id).second;
    if (valid) continue;
    warnings.push_back("node '" + printable(node.name) + "': invalid or duplicate object id; assigned a new identity");
    node.stable_id.clear();
  }
  return used;
}

std::string unused_number(std::set<std::string>& used, std::size_t& next) {
  while (used.count(std::to_string(next)) != 0) ++next;
  const std::string id = std::to_string(next++);
  used.insert(id);
  return id;
}

}  // namespace

bool valid_scene_id(const std::string& id) {
  return !id.empty() && id.size() <= kMaxSceneIdBytes && std::all_of(id.begin(), id.end(), id_character);
}

std::vector<std::string> assign_scene_ids(SceneDesc& scene) {
  std::vector<std::string> warnings;
  std::set<std::string> used = keep_unique_ids(scene, warnings);
  std::size_t next = 0;
  for (NodeDesc& node : scene.nodes) {
    if (node.stable_id.empty()) node.stable_id = unused_number(used, next);
  }
  return warnings;
}

std::string make_scene_id() {
  std::random_device random;
  std::uniform_int_distribution<std::uint64_t> distribution;
  char text[kSceneIdHexDigits + 1];
  std::snprintf(text, sizeof(text), "%0*llx", kSceneIdHexDigits, static_cast<unsigned long long>(distribution(random)));
  return text;
}

std::string scene_node_id(const SceneDesc& scene, int index) {
  const auto& id = scene.nodes[index].stable_id;
  return id.empty() ? std::to_string(index) : id;
}

}  // namespace redgem
