#include "editor/lod_levels.hpp"

#include <meshoptimizer.h>

#include <algorithm>
#include <cmath>

#include "editor/document/scene_document.hpp"
#include "scene/props.hpp"

namespace redgem::lod_levels {
namespace {

constexpr std::size_t kIndicesPerTriangle = 3;
constexpr std::size_t kFloatsPerPosition = 3;
constexpr float kNextLevelShrink = 0.5f;  // each level's switch is at half the size of the one before
// How far a simplification may move the surface, as a share of the mesh's extent.
constexpr float kTargetError = 0.05f;
constexpr float kSloppyError = 0.25f;
// A result this far above its target falls back to the coarser simplifier.
constexpr float kSloppyAbove = 1.5f;
// A level must have at most this share of the triangles of the one before, or it is not made.
constexpr float kLeastGain = 0.9f;
constexpr float kHalf = 0.5f;
constexpr const char* kLevelMark = " LOD ";

using Parts = std::vector<std::vector<std::uint32_t>>;

float boundary(int level, float first) { return first * std::pow(kNextLevelShrink, static_cast<float>(level - 1)); }

std::size_t target_of(std::size_t indices, float ratio) {
  const std::size_t whole = indices / kIndicesPerTriangle;
  const auto triangles = static_cast<std::size_t>(std::lround(static_cast<float>(whole) * ratio));
  return std::max<std::size_t>(1, triangles) * kIndicesPerTriangle;
}

std::size_t index_count(const Parts& parts) {
  std::size_t count = 0;
  for (const auto& part : parts) count += part.size();
  return count;
}

Parts own_indices(const MeshDesc& mesh) {
  Parts parts;
  for (const PrimitiveDesc& part : mesh.primitives) parts.push_back(part.indices);
  return parts;
}

Parts simplified_parts(const MeshDesc& mesh, const Parts& before, float ratio) {
  Parts parts;
  for (std::size_t part = 0; part < before.size(); ++part) {
    parts.push_back(simplified(mesh.primitives[part], before[part], ratio));
  }
  return parts;
}

// Each level's triangles after level 0, while each is lighter enough than the one before.
std::vector<Parts> lighter_levels(const MeshDesc& mesh, const import_settings::ModelSettings& settings) {
  std::vector<Parts> levels;
  Parts before = own_indices(mesh);
  for (int level = 1; level <= settings.lod_levels; ++level) {
    Parts next = simplified_parts(mesh, before, settings.lod_ratio);
    const bool lighter = static_cast<float>(index_count(next)) <= static_cast<float>(index_count(before)) * kLeastGain;
    if (!lighter) break;
    levels.push_back(next);
    before = std::move(next);
  }
  return levels;
}

float radius_of(const MeshDesc& mesh) { return length(mesh.bounds.max - mesh.bounds.min) * kHalf; }

PropertyMap level_props(ScreenRange range, float radius, float fade) {
  PropertyMap props{{props::kLodRadius, static_cast<double>(radius)}, {props::kLodFade, static_cast<double>(fade)}};
  if (range.from > 0.0f) props[props::kScreenSizeFrom] = static_cast<double>(range.from);
  if (std::isfinite(range.to)) props[props::kScreenSizeTo] = static_cast<double>(range.to);
  return props;
}

void mark_own_level(SceneDocument& document, int node, const PropertyMap& props) {
  for (const auto& [key, value] : props) document.set_prop(node, key, value, false);
}

// What one mesh node's levels share: how they are named and when each is drawn.
struct LevelPlan {
  std::string name;
  int last = 0;
  float radius = 0.0f;
  const import_settings::ModelSettings& settings;

  PropertyMap props(int level) const {
    return level_props(screen_range(level, last, settings.lod_screen_size), radius, settings.lod_fade);
  }
};

void add_levels(SceneDocument& document, int node, const std::vector<Parts>& levels, const LevelPlan& plan,
                LevelCounts& counts) {
  mark_own_level(document, node, plan.props(0));
  for (int level = 1; level <= plan.last; ++level) {
    const Parts& parts = levels[static_cast<std::size_t>(level - 1)];
    document.add_detail_level(node, parts, plan.name + kLevelMark + std::to_string(level), plan.props(level));
    counts.triangles.push_back(index_count(parts) / kIndicesPerTriangle);
  }
}

LevelCounts make_node_levels(SceneDocument& document, int node, const import_settings::ModelSettings& settings) {
  const std::string name = document.node(node).name;
  // (A copy: adding levels adds meshes.)
  const MeshDesc mesh = document.desc().meshes.at(static_cast<std::size_t>(document.node(node).mesh));
  LevelCounts counts{name, {triangles(mesh)}};
  if (!simplifiable(mesh)) return counts;
  const std::vector<Parts> levels = lighter_levels(mesh, settings);
  if (levels.empty()) return counts;
  add_levels(document, node, levels, LevelPlan{name, static_cast<int>(levels.size()), radius_of(mesh), settings},
             counts);
  return counts;
}

// The mesh nodes under `root` (itself included), before any level is added.
std::vector<int> mesh_nodes(const SceneDocument& document, int root) {
  std::vector<int> nodes{root};
  std::vector<int> found;
  for (std::size_t next = 0; next < nodes.size(); ++next) {
    if (document.node(nodes[next]).mesh != kNoIndex) found.push_back(nodes[next]);
    for (int child : document.node(nodes[next]).children) nodes.push_back(child);
  }
  return found;
}

// A child made as a level of detail of its parent's mesh.
bool is_level(const NodeDesc& node, const SceneDesc& desc) {
  return is_index_of(node.mesh, desc.meshes.size()) && node.props.count(props::kLodRadius) > 0;
}

}  // namespace

ScreenRange screen_range(int level, int levels, float first) {
  ScreenRange range;
  if (level > 0) range.to = boundary(level, first);
  if (level < levels) range.from = boundary(level + 1, first);
  return range;
}

std::vector<std::uint32_t> simplified(const PrimitiveDesc& part, const std::vector<std::uint32_t>& indices,
                                      float ratio) {
  const std::size_t target = target_of(indices.size(), ratio);
  const std::size_t vertices = part.positions.size() / kFloatsPerPosition;
  const std::size_t stride = kFloatsPerPosition * sizeof(float);
  std::vector<std::uint32_t> kept(indices.size());
  std::size_t count = meshopt_simplify(kept.data(), indices.data(), indices.size(), part.positions.data(), vertices,
                                       stride, target, kTargetError);
  if (static_cast<float>(count) > static_cast<float>(target) * kSloppyAbove) {
    count = meshopt_simplifySloppy(kept.data(), indices.data(), indices.size(), part.positions.data(), vertices, stride,
                                   target, kSloppyError);
  }
  kept.resize(count);
  return kept;
}

std::size_t triangles(const MeshDesc& mesh) {
  std::size_t count = 0;
  for (const PrimitiveDesc& part : mesh.primitives) count += part.indices.size() / kIndicesPerTriangle;
  return count;
}

bool simplifiable(const MeshDesc& mesh) {
  return std::none_of(mesh.primitives.begin(), mesh.primitives.end(), [](const PrimitiveDesc& part) {
    const bool moved = !part.joints.empty() || !part.targets.empty();
    return moved || part.indices.empty();
  });
}

std::vector<LevelCounts> make_levels(SceneDocument& document, int root,
                                     const import_settings::ModelSettings& settings) {
  std::vector<LevelCounts> counts;
  if (settings.lod_levels == import_settings::kNoLods) return counts;
  for (int node : mesh_nodes(document, root)) counts.push_back(make_node_levels(document, node, settings));
  return counts;
}

std::vector<std::size_t> level_triangles(const SceneDesc& desc, int node) {
  std::vector<std::size_t> counts;
  const NodeDesc& owner = desc.nodes.at(static_cast<std::size_t>(node));
  if (!is_index_of(owner.mesh, desc.meshes.size())) return counts;
  counts.push_back(triangles(desc.meshes[static_cast<std::size_t>(owner.mesh)]));
  for (int child : owner.children) {
    const NodeDesc& level = desc.nodes.at(static_cast<std::size_t>(child));
    if (is_level(level, desc)) counts.push_back(triangles(desc.meshes[static_cast<std::size_t>(level.mesh)]));
  }
  return counts;
}

std::string counts_list(const std::vector<std::size_t>& counts) {
  std::string text;
  for (std::size_t count : counts) text += (text.empty() ? "" : ", ") + std::to_string(count);
  return text;
}

std::string counts_text(const std::vector<LevelCounts>& counts) {
  std::string text;
  for (const LevelCounts& node : counts) text += node.node + ": " + counts_list(node.triangles) + " triangles\n";
  return text;
}

}  // namespace redgem::lod_levels
