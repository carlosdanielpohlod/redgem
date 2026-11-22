// Part of SceneDocument: levels of detail, lighter copies of a mesh that share its vertices.

#include "editor/document/scene_document.hpp"

#include "editor/document/glb_geometry.hpp"
#include "editor/node_tree.hpp"

namespace redgem {
namespace {

// The node's mesh with other triangles in each part (its bounds kept: the vertices are the same).
MeshDesc mesh_level(const MeshDesc& mesh, const std::vector<std::vector<std::uint32_t>>& indices,
                    const std::string& name) {
  MeshDesc level = mesh;
  level.name = name;
  for (std::size_t part = 0; part < indices.size(); ++part) level.primitives[part].indices = indices[part];
  return level;
}

}  // namespace

int SceneDocument::add_detail_level(int node, const std::vector<std::vector<std::uint32_t>>& indices,
                                    const std::string& name, const PropertyMap& props) {
  const int mesh = has_node(node) ? desc_.nodes[node].mesh : kNoIndex;
  if (mesh == kNoIndex) return kNoIndex;
  const MeshDesc& source = desc_.meshes.at(static_cast<std::size_t>(mesh));
  if (source.primitives.size() != indices.size()) return kNoIndex;
  const auto written = glb_geometry::append_mesh_level(glb_, mesh, indices, name);
  if (!written) return kNoIndex;
  desc_.meshes.push_back(mesh_level(source, indices, name));
  NodeDesc level = node_tree::named_node(name, node);
  level.mesh = static_cast<int>(desc_.meshes.size()) - 1;
  level.props = props;
  return append_node(std::move(level));
}

}  // namespace redgem
