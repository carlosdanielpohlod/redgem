#pragma once

// Levels of detail made for an imported model (its import settings' lod_* fields, see
// core/import_settings.hpp): each mesh is simplified with meshoptimizer into lighter copies that
// share its vertices, each a child of the mesh's node drawn at a smaller size on screen.

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "core/import_settings.hpp"
#include "scene/scene_desc.hpp"

namespace redgem {
class SceneDocument;
}

namespace redgem::lod_levels {

inline constexpr float kNoScreenLimit = std::numeric_limits<float>::infinity();

// The sizes on screen a level is drawn at (shares of the view's height its bounding sphere covers).
struct ScreenRange {
  float from = 0.0f;
  float to = kNoScreenLimit;
};

// Level `level` of 0..levels: level 1 starts below `first`, each next one at half the size of the
// one before.
ScreenRange screen_range(int level, int levels, float first);

// For each vertex of a part, 1 when it must stay put (where the part meets another), else 0.
using VertexLocks = std::vector<unsigned char>;

// Triangles of a part (`indices` into its vertices) simplified toward `ratio` of them: indices
// into the same vertices. The surface moves little and `locks` (empty: none) stay put, so a shape
// may keep more triangles than asked (or lose them all, when the part is tiny).
std::vector<std::uint32_t> simplified(const PrimitiveDesc& part, const std::vector<std::uint32_t>& indices, float ratio,
                                      const VertexLocks& locks = {});

std::size_t triangles(const MeshDesc& mesh);
// Whether a mesh can be simplified: no skeleton and no shape keys move its vertices.
bool simplifiable(const MeshDesc& mesh);

// How many triangles each level of one mesh node has, level 0 first.
struct LevelCounts {
  std::string node;
  std::vector<std::size_t> triangles;
};

// Makes the levels of every mesh node under `root` (unrecorded: part of the import's undo step),
// and gives every level, the node's own mesh included, the props that say when it is drawn. Each
// level copies the node's draw props (tint, shadows, sway, visibility). A level that would not be
// lighter than the one before, or would lose a part, is not made.
std::vector<LevelCounts> make_levels(SceneDocument& document, int root, const import_settings::ModelSettings& settings);

// How many triangles a mesh node shows at each of its levels of detail: its own mesh first, then
// each child level made for it. Empty for a node without a mesh.
std::vector<std::size_t> level_triangles(const SceneDesc& desc, int node);
// "1200, 600, 300": each level's count.
std::string counts_list(const std::vector<std::size_t>& counts);

// "oak: 1200, 600, 300 triangles" for each mesh node.
std::string counts_text(const std::vector<LevelCounts>& counts);

}  // namespace redgem::lod_levels
