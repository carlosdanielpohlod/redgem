#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include "scene/scene_desc.hpp"
#include "scene/props.hpp"

namespace redgem::prefabs {
inline constexpr const char* kFileProperty = props::kPrefabFile;
// Bounds apply to one complete expansion, including nested references and cached files.
struct Limits {
  static constexpr std::size_t kDefaultFiles = 64;
  static constexpr std::size_t kDefaultDepth = 8;
  static constexpr std::size_t kDefaultInstances = 4096;
  static constexpr std::size_t kDefaultNodes = 100000;
  static constexpr std::uint64_t kMebibyte = 1024 * 1024;
  static constexpr std::uint64_t kDefaultInputBytes = 256 * kMebibyte;
  static constexpr std::uint64_t kDefaultExpandedBytes = 512 * kMebibyte;
  std::size_t files = kDefaultFiles;
  std::size_t depth = kDefaultDepth;
  std::size_t instances = kDefaultInstances;
  std::size_t nodes = kDefaultNodes;
  std::uint64_t input_bytes = kDefaultInputBytes;
  std::uint64_t expanded_bytes = kDefaultExpandedBytes;
};
// Resolves game-root-relative GLB references; throws on unusable dependencies. The caller's
// original description is untouched. Shared files contribute meshes/materials/images once.
SceneDesc resolve(SceneDesc scene, const std::filesystem::path& game_dir, const Limits& limits = {});
bool has_references(const SceneDesc& scene);
}  // namespace redgem::prefabs
