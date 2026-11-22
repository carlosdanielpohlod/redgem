#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include "scene/scene_desc.hpp"
#include "scene/props.hpp"

namespace redgem::prefabs {
inline constexpr const char* kFileProperty = props::kPrefabFile;
// game.json's key that raises the byte budgets below, in MiB: {"source": 1024, "decoded": 2048}.
inline constexpr const char* kBudgetKey = "prefab_budget_mb";
// The most either budget may be, in MiB: a scene's own byte budget (gltf_budget::kMaxSceneBytes).
inline constexpr std::uint64_t kMostBudgetMb = 4096;
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
  // Decoded bytes already spent before the expansion starts (the editor's copy of the scene).
  std::uint64_t spent_bytes = 0;
  // Whether game.json's kBudgetKey sets the byte budgets, so running out of one names it.
  bool from_game = true;
};
// Editor caches retain source defaults and ordered root offsets for cheap placement edits.
struct Origin {
  std::string reference;
  std::string name;
  PropertyMap defaults;
  std::vector<Transform> roots;
  // The inherited parts by path (scene/prefab_overrides.hpp), as resolved node indices.
  std::map<std::string, int> parts;
  // Authored children hung from an inherited part (props::kPrefabParent): the placement-relative
  // transform of that part, which the preview combines with the child's own.
  std::map<int, Transform> attached;
};
using Origins = std::map<int, Origin>;
Transform inherited_local(const Origin& origin, Transform placement);
PropertyMap inherited_props(const Origin& origin, const PropertyMap& authored);
// Resolves game-root-relative GLB references; throws on unusable dependencies. The caller's
// original description is untouched. Shared files contribute meshes/materials/images once.
// Each placement's overrides (props::kPrefabOverrides) are applied, removed parts left out and
// children hung from inherited parts (props::kPrefabParent) moved under them. With `origins` (the
// editor's preview), authored nodes keep their indices: such a child stays under its placement,
// its transform combined with the part's, and only inherited nodes are left out.
SceneDesc resolve(SceneDesc scene, const std::filesystem::path& game_dir, const Limits& limits = {},
                  Origins* origins = nullptr);
bool has_references(const SceneDesc& scene);
}  // namespace redgem::prefabs
