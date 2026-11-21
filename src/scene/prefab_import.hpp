#pragma once
#include <cstdint>
#include <string>
#include "scene/prefab_overrides.hpp"
#include "scene/prefabs.hpp"

namespace redgem::prefabs {
inline constexpr std::size_t kIdentityBytes = 64;
Transform checked_placement(const Transform& placement);
void record_origin(Origins& origins, int id, const NodeDesc& authored, const NodeDesc& source);
struct Assets {
  int images = 0;
  int materials = 0;
  int meshes = 0;
};
struct Source {
  SceneDesc scene;
  Assets assets;
  std::vector<std::size_t> animations;
  std::uint64_t instance_bytes = 0;
  std::uint64_t asset_bytes = 0;
  bool transferred = false;
};
struct Instance {
  int root = kNoIndex;
  int nodes = 0;
  int skins = 0;
  std::string identity;
  std::string file;
};
std::uint64_t instance_bytes(const SceneDesc& scene);
std::uint64_t asset_bytes(const SceneDesc& scene);
void transfer_assets(SceneDesc& target, Source& source);
int instance_node(int node, const Instance& instance);
std::string child_identity(const Instance& instance, const std::string& source_identity);
void append_animations(SceneDesc& target, Source& source, const Instance& instance);
Instance append_instance(SceneDesc& target, Source& source, int root, const std::string& file);

// Overrides (prefab_customize.cpp). `part_ids` gives the source identity of each inherited node.
using PartPaths = std::map<std::string, int>;
PartPaths part_paths(const SceneDesc& scene, int placement, const std::vector<int>& own,
                     const std::map<int, std::string>& part_ids);
// Applies what it can; returns the parts to leave out. Missing parts are reported and skipped.
std::vector<int> apply_part_overrides(SceneDesc& scene, const PartPaths& parts, const Overrides& overrides);
// Whether `part` is one of `roots` or under one, below `placement`.
bool inside_any(const SceneDesc& scene, int part, const std::vector<int>& roots, int placement);
// A part's transform relative to the placement above it.
Transform part_chain(const SceneDesc& scene, int placement, int part);
// Moves `child` from its parent to under `part`.
void hang_under(SceneDesc& scene, int child, int part);
// Leaves out `removed` with everything under them and puts parents before children again.
// Returns old index -> new index (kNoIndex for a node left out); skins and clips follow.
std::vector<int> compact_nodes(SceneDesc& scene, const std::vector<int>& removed);
}  // namespace redgem::prefabs
