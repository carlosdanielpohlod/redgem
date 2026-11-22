#include "editor/lod_levels.hpp"

#include <meshoptimizer.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>

#include "editor/document/scene_document.hpp"
#include "scene/props.hpp"

namespace redgem::lod_levels {
namespace {

constexpr std::size_t kIndicesPerTriangle = 3;
constexpr std::size_t kFloatsPerPosition = 3;
constexpr float kNextLevelShrink = 0.5f;  // each level's switch is at half the size of the one before
// How far a simplification may move the surface, as a share of the mesh's extent.
constexpr float kTargetError = 0.05f;
constexpr unsigned int kSimplifyOptions = 0;
constexpr std::size_t kNoAttributes = 0;
constexpr unsigned char kLocked = 1;
// A level must have at most this share of the triangles of the one before, or it is not made.
constexpr float kLeastGain = 0.9f;
constexpr float kHalf = 0.5f;
constexpr const char* kLevelMark = " LOD ";
// What a level copies of its mesh node's props, so that it looks and behaves the same when drawn.
constexpr const char* kDrawnProps[] = {props::kTint,        props::kCastShadow, props::kSway,     props::kVisible,
                                       props::kVisibleFrom, props::kVisibleTo,  props::kViewModel};

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

using Position = std::array<float, kFloatsPerPosition>;
constexpr int kSeveralParts = -1;

Position position_of(const PrimitiveDesc& part, std::size_t vertex) {
  Position position{};
  std::copy_n(part.positions.begin() + static_cast<std::ptrdiff_t>(vertex * kFloatsPerPosition), kFloatsPerPosition,
              position.begin());
  return position;
}

// For each position of the mesh, the part it is in (kSeveralParts when more than one has it).
std::map<Position, int> parts_at(const MeshDesc& mesh) {
  std::map<Position, int> owners;
  for (std::size_t part = 0; part < mesh.primitives.size(); ++part) {
    const PrimitiveDesc& primitive = mesh.primitives[part];
    for (std::size_t vertex = 0; vertex * kFloatsPerPosition < primitive.positions.size(); ++vertex) {
      const auto [found, added] = owners.emplace(position_of(primitive, vertex), static_cast<int>(part));
      if (!added && found->second != static_cast<int>(part)) found->second = kSeveralParts;
    }
  }
  return owners;
}

// For each part, its vertices where another part meets it: they stay put, so the parts still meet.
std::vector<VertexLocks> meeting_vertices(const MeshDesc& mesh) {
  const std::map<Position, int> owners = parts_at(mesh);
  std::vector<VertexLocks> locks;
  for (const PrimitiveDesc& primitive : mesh.primitives) {
    VertexLocks part(primitive.positions.size() / kFloatsPerPosition, 0);
    for (std::size_t vertex = 0; vertex < part.size(); ++vertex) {
      if (owners.at(position_of(primitive, vertex)) == kSeveralParts) part[vertex] = kLocked;
    }
    locks.push_back(std::move(part));
  }
  return locks;
}

Parts simplified_parts(const MeshDesc& mesh, const std::vector<VertexLocks>& locks, const Parts& before, float ratio) {
  Parts parts;
  for (std::size_t part = 0; part < before.size(); ++part) {
    parts.push_back(simplified(mesh.primitives[part], before[part], ratio, locks[part]));
  }
  return parts;
}

bool lost_a_part(const Parts& parts) {
  return std::any_of(parts.begin(), parts.end(), [](const auto& part) { return part.empty(); });
}

// Whether a level is worth making after `before`: lighter enough, and with something of every part.
bool worth_making(const Parts& next, const Parts& before) {
  const bool lighter = static_cast<float>(index_count(next)) <= static_cast<float>(index_count(before)) * kLeastGain;
  return lighter && !lost_a_part(next);
}

// Each level's triangles after level 0, while each is worth making.
std::vector<Parts> lighter_levels(const MeshDesc& mesh, const import_settings::ModelSettings& settings) {
  std::vector<Parts> levels;
  Parts before = own_indices(mesh);
  const std::vector<VertexLocks> locks = meeting_vertices(mesh);
  for (int level = 1; level <= settings.lod_levels; ++level) {
    Parts next = simplified_parts(mesh, locks, before, settings.lod_ratio);
    if (!worth_making(next, before)) break;
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

void set_props(SceneDocument& document, int node, const PropertyMap& props) {
  for (const auto& [key, value] : props) document.set_prop(node, key, value, false);
}

PropertyMap drawn_props(const NodeDesc& node) {
  PropertyMap props;
  for (const char* key : kDrawnProps) {
    if (const auto found = node.props.find(key); found != node.props.end()) props.insert(*found);
  }
  return props;
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

// The levels the document takes, in order (it may refuse one whose parts the file keeps apart).
std::vector<int> add_children(SceneDocument& document, int node, const std::vector<Parts>& levels,
                              const std::string& name) {
  const PropertyMap props = drawn_props(document.node(node));
  std::vector<int> made;
  for (std::size_t level = 1; level <= levels.size(); ++level) {
    const int child =
        document.add_detail_level(node, levels[level - 1], name + kLevelMark + std::to_string(level), props);
    if (child == kNoIndex) break;
    made.push_back(child);
  }
  return made;
}

// Says when the node and each level made for it are drawn: only once they are all there, so a
// node whose levels were refused is drawn at every size.
void mark_levels(SceneDocument& document, int node, const std::vector<int>& made, const LevelPlan& plan) {
  set_props(document, node, plan.props(0));
  for (std::size_t level = 1; level <= made.size(); ++level) {
    PropertyMap props = plan.props(static_cast<int>(level));
    props[props::kLodLevel] = static_cast<double>(level);
    set_props(document, made[level - 1], props);
  }
}

void add_levels(SceneDocument& document, int node, const std::vector<Parts>& levels, LevelPlan plan,
                LevelCounts& counts) {
  const std::vector<int> made = add_children(document, node, levels, plan.name);
  if (made.empty()) return;
  plan.last = static_cast<int>(made.size());
  mark_levels(document, node, made, plan);
  for (std::size_t level = 0; level < made.size(); ++level) {
    counts.triangles.push_back(index_count(levels[level]) / kIndicesPerTriangle);
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
  add_levels(document, node, levels, LevelPlan{name, 0, radius_of(mesh), settings}, counts);
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
  return is_index_of(node.mesh, desc.meshes.size()) && node.props.count(props::kLodLevel) > 0;
}

}  // namespace

ScreenRange screen_range(int level, int levels, float first) {
  ScreenRange range;
  if (level > 0) range.to = boundary(level, first);
  if (level < levels) range.from = boundary(level + 1, first);
  return range;
}

std::vector<std::uint32_t> simplified(const PrimitiveDesc& part, const std::vector<std::uint32_t>& indices, float ratio,
                                      const VertexLocks& locks) {
  const std::size_t target = target_of(indices.size(), ratio);
  const std::size_t vertices = part.positions.size() / kFloatsPerPosition;
  const unsigned char* locked = locks.size() == vertices ? locks.data() : nullptr;
  std::vector<std::uint32_t> kept(indices.size());
  const std::size_t count = meshopt_simplifyWithAttributes(
      kept.data(), indices.data(), indices.size(), part.positions.data(), vertices, kFloatsPerPosition * sizeof(float),
      nullptr, 0, nullptr, kNoAttributes, locked, target, kTargetError, kSimplifyOptions, nullptr);
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
