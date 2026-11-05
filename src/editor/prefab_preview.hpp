#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include "scene/prefabs.hpp"

namespace redgem {
// The document stays authored; this disposable snapshot supplies inherited rendering data.
class PrefabPreview {
 public:
  void rebuild(const SceneDesc& document, const std::filesystem::path& game_dir, const prefabs::Limits& limits = {});
  const SceneDesc* resolved() const { return resolved_.get(); }
  bool refresh(const SceneDesc& document, int node);
  bool sync_assets(const SceneDesc& document, const std::vector<int>& changed_meshes);
  std::uint64_t generation() const { return generation_; }

 private:
  static constexpr std::size_t kAssetKinds = 3;  // meshes, materials and images determine import offsets
  using Counts = std::array<std::size_t, kAssetKinds>;
  static Counts counts(const SceneDesc& scene);
  void load(const SceneDesc& document, const std::filesystem::path& game_dir, const prefabs::Limits& limits);
  void report(const std::string& error);
  void remember_references(const SceneDesc& document);
  bool reference_unchanged(const NodeDesc& node, int id) const;
  bool refresh_root_node(const NodeDesc& authored, int node, const prefabs::Origin& origin);
  std::unique_ptr<SceneDesc> resolved_;
  prefabs::Origins origins_;
  std::map<int, PropertyValue> references_;
  Counts base_counts_{};
  std::uint64_t generation_ = 0;
  std::string error_;
};
}  // namespace redgem
