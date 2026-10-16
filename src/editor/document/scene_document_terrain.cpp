// Part of SceneDocument: terrains, the one kind of mesh whose vertices the editor reshapes.

#include "editor/document/scene_document.hpp"

#include <cstring>

#include "editor/document/glb_geometry.hpp"
#include "editor/document/gltf_json.hpp"
#include "scene/props.hpp"

namespace redgem {
namespace {

using namespace gltf_json;

constexpr int kOnlyPrimitive = 0;  // a terrain's mesh is a single piece

// A node that says it is a terrain, and has a mesh to be one with.
bool marked_terrain(const NodeDesc& node) {
  return get_bool(node.props, props::kTerrain).value_or(false) && node.mesh != kNoIndex;
}

// A mesh of one piece, with the vertices of that grid.
bool shaped_like(const MeshDesc& mesh, const TerrainGrid& grid) {
  return mesh.primitives.size() == 1 && grid.fits(mesh.primitives.front());
}

}  // namespace

std::optional<TerrainGrid> SceneDocument::terrain_grid(int node) const {
  if (!has_node(node)) return std::nullopt;
  const NodeDesc& desc = desc_.nodes[static_cast<std::size_t>(node)];
  if (!marked_terrain(desc)) return std::nullopt;
  const TerrainGrid grid{static_cast<float>(get_number(desc.props, props::kTerrainSize).value_or(0.0)),
                         static_cast<int>(get_number(desc.props, props::kTerrainCells).value_or(0.0))};
  return shaped_like(desc_.meshes.at(static_cast<std::size_t>(desc.mesh)), grid) ? std::optional<TerrainGrid>(grid)
                                                                                 : std::nullopt;
}

bool SceneDocument::sculpt_terrain(int node, const TerrainBrush& brush) {
  const auto grid = terrain_grid(node);
  if (!grid) return false;
  const int mesh = desc_.nodes[static_cast<std::size_t>(node)].mesh;
  MeshDesc& target = desc_.meshes[static_cast<std::size_t>(mesh)];
  if (!redgem::sculpt_terrain(target.primitives.front(), *grid, brush)) return false;
  mark_reshaped(mesh);
  touch();
  return true;
}

void SceneDocument::mark_reshaped(int mesh) {
  MeshDesc& target = desc_.meshes.at(static_cast<std::size_t>(mesh));
  target.bounds = mesh_bounds_of(target.primitives.front());
  changed_meshes_.insert(mesh);
  unsaved_meshes_.insert(mesh);
}

void SceneDocument::restore_shapes(const std::map<int, MeshShape>& shapes) {
  for (const auto& [mesh, shape] : shapes) {
    MeshDesc& target = desc_.meshes.at(static_cast<std::size_t>(mesh));
    PrimitiveDesc& ground = target.primitives.front();
    if (ground.positions == shape.positions) continue;
    ground.positions = shape.positions;
    ground.normals = shape.normals;
    mark_reshaped(mesh);
  }
}

std::vector<int> SceneDocument::take_changed_meshes() {
  std::vector<int> changed(changed_meshes_.begin(), changed_meshes_.end());
  changed_meshes_.clear();
  return changed;
}

bool SceneDocument::store_mesh_vertices(int mesh, std::string& error) {
  const MeshDesc& source = desc_.meshes.at(static_cast<std::size_t>(mesh));
  const nlohmann::json& attributes =
      glb_.json[kMeshes][static_cast<std::size_t>(mesh)][kPrimitives][kOnlyPrimitive][kAttributes];
  const auto positions = attributes.at(kPositionAttribute).get<std::size_t>();
  const auto normals = attributes.at(kNormalAttribute).get<std::size_t>();
  if (!store_shape(positions, normals, source.primitives.front())) {
    error =
        "cannot save the shape of '" + source.name + "': its vertices are not stored the way the editor writes them";
    return false;
  }
  glb_geometry::write_bounds(glb_.json[kAccessors][positions], source.bounds);
  return true;
}

bool SceneDocument::store_shape(std::size_t positions, std::size_t normals, const PrimitiveDesc& ground) {
  return store_stream(positions, ground.positions) && store_stream(normals, ground.normals);
}

bool SceneDocument::store_stream(std::size_t accessor_index, const std::vector<float>& values) {
  // (Where the bytes go is worked out from what the file says, and checked against the chunk itself.)
  const std::optional<std::size_t> offset =
      glb_geometry::vec3_stream_offset(glb_.json, accessor_index, values.size(), glb_.bin.size());
  if (!offset) return false;
  std::memcpy(glb_.bin.data() + *offset, values.data(), values.size() * sizeof(float));
  return true;
}

}  // namespace redgem
