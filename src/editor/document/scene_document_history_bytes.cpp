// Part of SceneDocument: how much the undo history keeps alive that nothing else does. An
// estimate of what grows with it: each step's copy of the objects (their names, properties and
// children too) and of every terrain's shape, and what only the steps still use of deleted
// objects and undone imports: their vertices, their pictures, and their bytes in the scene's file.

#include <algorithm>
#include <cstdint>
#include <exception>
#include <variant>

#include "editor/document/glb_usage.hpp"
#include "editor/document/gltf_json.hpp"
#include "editor/document/scene_document.hpp"

namespace redgem {
namespace {

using namespace glb_usage;

// What one entry of a property map takes besides its key and value: the links of the tree it is in.
constexpr std::size_t kMapEntryLinks = 4 * sizeof(void*);

// Notes which meshes the nodes show.
void mark_meshes(const std::vector<NodeDesc>& nodes, std::vector<bool>& used) {
  for (const NodeDesc& node : nodes) {
    if (is_index_of(node.mesh, used.size())) used[static_cast<std::size_t>(node.mesh)] = true;
  }
}

// Something some step of the history uses, and nothing now: kept only for undo and redo.
bool kept_for_undo(bool in_history, bool in_use) { return in_history && !in_use; }

std::size_t property_bytes(const PropertyMap& props) {
  std::size_t bytes = 0;
  for (const auto& [key, value] : props) {
    bytes += sizeof(PropertyMap::value_type) + kMapEntryLinks + key.capacity();
    if (const auto* text = std::get_if<std::string>(&value)) bytes += text->capacity();
  }
  return bytes;
}

std::size_t node_bytes(const NodeDesc& node) {
  return sizeof(NodeDesc) + node.name.capacity() + node.children.capacity() * sizeof(int) + property_bytes(node.props);
}

std::size_t primitive_bytes(const PrimitiveDesc& part) {
  const std::size_t floats = part.positions.size() + part.normals.size() + part.texcoords.size() + part.weights.size();
  return floats * sizeof(float) + part.indices.size() * sizeof(std::uint32_t) + part.colors.size() +
         part.joints.size() * sizeof(std::uint16_t);
}

std::size_t mesh_bytes(const MeshDesc& mesh) {
  std::size_t bytes = 0;
  for (const PrimitiveDesc& part : mesh.primitives) bytes += primitive_bytes(part);
  return bytes;
}

// The bytes of the file's buffer views that `kept` reaches and `used` does not.
std::size_t view_bytes(const nlohmann::json& doc, const Usage& kept, const Usage& used) {
  const nlohmann::json views = gltf_json::list_of(doc, gltf_json::kBufferViews);
  std::size_t bytes = 0;
  for (std::size_t view = 0; view < views.size(); ++view) {
    if (!kept_for_undo(kept[kBufferViewKind][view], used[kBufferViewKind][view])) continue;
    bytes += gltf_json::size_from(views[view].value(gltf_json::kByteLength, nlohmann::json())).value_or(0);
  }
  return bytes;
}

}  // namespace

std::vector<bool> SceneDocument::meshes_in_use() const {
  std::vector<bool> used(desc_.meshes.size(), false);
  mark_meshes(desc_.nodes, used);
  return used;
}

std::vector<bool> SceneDocument::meshes_in_history() const {
  std::vector<bool> used(desc_.meshes.size(), false);
  for (const auto& [mesh, steps] : steps_showing_) {
    if (is_index_of(mesh, used.size())) used[static_cast<std::size_t>(mesh)] = true;
  }
  return used;
}

std::size_t SceneDocument::bytes_of(const Snapshot& step) {
  std::size_t bytes = 0;
  for (const NodeDesc& node : step.nodes) bytes += node_bytes(node);
  for (const MaterialDesc& material : step.materials) bytes += sizeof(MaterialDesc) + material.name.capacity();
  for (const auto& [mesh, shape] : step.shapes) {
    bytes += (shape.positions.capacity() + shape.normals.capacity()) * sizeof(float);
  }
  return bytes;
}

std::size_t SceneDocument::history_bytes() { return steps_bytes_ + kept_bytes(); }

std::size_t SceneDocument::kept_bytes() {
  const std::vector<bool> in_use = meshes_in_use();
  std::vector<bool> only_in_history = meshes_in_history();
  std::transform(only_in_history.begin(), only_in_history.end(), in_use.begin(), only_in_history.begin(),
                 kept_for_undo);
  return vertex_bytes(only_in_history) + file_bytes(only_in_history, in_use) + undo_picture_bytes();
}

std::size_t SceneDocument::vertex_bytes(const std::vector<bool>& meshes) const {
  std::size_t bytes = 0;
  for (std::size_t mesh = 0; mesh < meshes.size(); ++mesh) {
    if (meshes[mesh]) bytes += mesh_bytes(desc_.meshes[mesh]);
  }
  return bytes;
}

std::size_t SceneDocument::file_bytes(const std::vector<bool>& kept, const std::vector<bool>& used) {
  if (std::none_of(kept.begin(), kept.end(), [](bool one) { return one; })) return 0;
  try {
    const Usage kept_usage = usage_of_meshes(glb_.json, kept);
    const Usage used_usage = usage_of_meshes(glb_.json, used);
    return view_bytes(glb_.json, kept_usage, used_usage) + picture_bytes(kept_usage, used_usage);
  } catch (const std::exception&) {
    return 0;  // (a file that points at its data in ways not followed here: its vertices are counted)
  }
}

std::size_t SceneDocument::picture_bytes(const glb_usage::Usage& kept, const glb_usage::Usage& used) const {
  const std::size_t images = std::min(desc_.images.size(), kept[kImageKind].size());
  std::size_t bytes = 0;
  for (std::size_t image = 0; image < images; ++image) {
    if (kept_for_undo(kept[kImageKind][image], used[kImageKind][image]))
      bytes += desc_.images[image].encoded.capacity();
  }
  return bytes;
}

}  // namespace redgem
