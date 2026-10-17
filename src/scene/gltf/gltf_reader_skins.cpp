// Skeletons of a glTF file.

#include <array>

#include "scene/gltf/gltf_checks.hpp"
#include "scene/gltf/gltf_reader.hpp"
#include "scene/scene_config.hpp"

namespace redgem {
namespace {

constexpr std::array<float, kFloatsPerMatrix> kIdentityMatrix = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

// Whether an accessor holds a 4x4 matrix of floats for each of `joints` joints.
bool holds_a_matrix_for_each(const cgltf_accessor* matrices, cgltf_size joints) {
  return gltf_checks::readable(matrices) && matrices->type == cgltf_type_mat4 &&
         matrices->component_type == cgltf_component_type_r_32f && matrices->count >= joints;
}

// The accessor holding a skin's inverse bind matrices, when it has one for each of `joints`
// joints; null otherwise (the joints then get the identity).
const cgltf_accessor* inverse_binds_of(const cgltf_skin& skin, cgltf_size joints) {
  const cgltf_accessor* matrices = skin.inverse_bind_matrices;
  return holds_a_matrix_for_each(matrices, joints) ? matrices : nullptr;
}

std::array<float, kFloatsPerMatrix> inverse_bind_of(const cgltf_accessor* matrices, cgltf_size joint) {
  std::array<float, kFloatsPerMatrix> matrix = kIdentityMatrix;
  if (matrices != nullptr) cgltf_accessor_read_float(matrices, joint, matrix.data(), matrix.size());
  return matrix;
}

// A skin that gives inverse bind matrices, but not one that can be used for each of its joints.
bool has_unusable_inverse_binds(const cgltf_skin& skin) {
  return skin.inverse_bind_matrices != nullptr && inverse_binds_of(skin, skin.joints_count) == nullptr;
}

// Whether a node is a mesh its scene has, deformed by a skeleton its scene has.
bool skinned_mesh(const NodeDesc& node, const SceneDesc& scene) {
  return is_index_of(node.mesh, scene.meshes.size()) && is_index_of(node.skin, scene.skins.size());
}

}  // namespace

void GltfReader::load_skins() {
  const std::vector<int> nodes = node_indices();
  for (cgltf_size i = 0; i < data_.skins_count; ++i) {
    warn_of_unused_parts(data_.skins[i], i);
    result_.scene.skins.push_back(load_skin(data_.skins[i], nodes));
  }
  for (NodeDesc& node : result_.scene.nodes) {
    if (node.source_node == kNoIndex) continue;
    node.skin = index_of(data_.nodes[node.source_node].skin, data_.skins);
  }
}

SkinDesc GltfReader::load_skin(const cgltf_skin& skin, const std::vector<int>& nodes) const {
  SkinDesc out;
  const cgltf_size joints = std::min<cgltf_size>(skin.joints_count, config::kMaxSkinJoints);
  const cgltf_accessor* matrices = inverse_binds_of(skin, joints);
  for (cgltf_size j = 0; j < joints; ++j) {
    out.joints.push_back(nodes[static_cast<std::size_t>(index_of(skin.joints[j], data_.nodes))]);
    out.inverse_bind.push_back(inverse_bind_of(matrices, j));
  }
  return out;
}

void GltfReader::warn_of_unused_parts(const cgltf_skin& skin, cgltf_size index) {
  if (has_unusable_inverse_binds(skin)) {
    warn("skin " + std::to_string(index) + " has no usable inverse bind matrix for each of its joints: none is used");
  }
  if (skin.joints_count <= config::kMaxSkinJoints) return;
  warn("skin " + std::to_string(index) + " has more than " + std::to_string(config::kMaxSkinJoints) +
       " joints: vertices of the extra ones follow its first");
}

void GltfReader::keep_joints_within_skins() {
  SceneDesc& scene = result_.scene;
  for (const NodeDesc& node : scene.nodes) {
    if (!skinned_mesh(node, scene)) continue;
    const std::size_t joints = scene.skins[static_cast<std::size_t>(node.skin)].joints.size();
    for (PrimitiveDesc& primitive : scene.meshes[static_cast<std::size_t>(node.mesh)].primitives) {
      keep_joints_within(primitive, joints);
    }
  }
}

}  // namespace redgem
