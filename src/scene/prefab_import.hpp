#pragma once
#include <cstdint>
#include <string>
#include "scene/prefabs.hpp"

namespace redgem::prefabs {
inline constexpr std::size_t kIdentityBytes = 64;
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
}  // namespace redgem::prefabs
