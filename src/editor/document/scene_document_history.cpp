// Part of SceneDocument: undo and redo. Every checkpoint is a snapshot of the nodes, the
// materials and the shape of every terrain.

#include "editor/document/scene_document.hpp"

#include <algorithm>

#include "editor/document/material_textures.hpp"

namespace redgem {
namespace {

bool same_color(const Color& a, const Color& b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }

bool same_vec3(Vec3 a, Vec3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }

bool same_coat(const ClearcoatDesc& a, const ClearcoatDesc& b) {
  return a.factor == b.factor && a.roughness == b.roughness && a.factor_image == b.factor_image &&
         a.roughness_image == b.roughness_image && a.normal_image == b.normal_image && a.normal_scale == b.normal_scale;
}

bool same_water(const WaterDesc& a, const WaterDesc& b) {
  return a.enabled == b.enabled && a.wave_height == b.wave_height && a.wavelength == b.wavelength &&
         a.speed == b.speed && same_vec3(a.absorption, b.absorption) && a.foam_width == b.foam_width &&
         a.refraction == b.refraction;
}

// The pictures of its texture slots.
bool same_pictures(const MaterialDesc& a, const MaterialDesc& b) {
  return material_textures::images_of(a) == material_textures::images_of(b);
}

bool same_material(const MaterialDesc& x, const MaterialDesc& y) {
  return same_color(x.base_color, y.base_color) && x.metallic == y.metallic && x.roughness == y.roughness &&
         same_vec3(x.emissive, y.emissive) && x.name == y.name && same_coat(x.clearcoat, y.clearcoat) &&
         same_water(x.water, y.water) && same_pictures(x, y);
}

bool same_materials(const std::vector<MaterialDesc>& a, const std::vector<MaterialDesc>& b) {
  return std::equal(a.begin(), a.end(), b.begin(), b.end(), same_material);
}

bool same_light(const std::optional<LightDesc>& a, const std::optional<LightDesc>& b) {
  if (a.has_value() != b.has_value()) return false;
  return !a || (a->type == b->type && a->color.r == b->color.r && a->color.g == b->color.g &&
                a->color.b == b->color.b && a->intensity == b->intensity && a->range == b->range);
}

bool same_camera(const std::optional<CameraDesc>& a, const std::optional<CameraDesc>& b) {
  if (a.has_value() != b.has_value()) return false;
  return !a || (a->fov_y_radians == b->fov_y_radians && a->near_plane == b->near_plane && a->far_plane == b->far_plane);
}

bool same_transform(const Transform& a, const Transform& b) {
  return same_vec3(a.position, b.position) && a.rotation.x == b.rotation.x && a.rotation.y == b.rotation.y &&
         a.rotation.z == b.rotation.z && a.rotation.w == b.rotation.w && same_vec3(a.scale, b.scale);
}

// What undo keeps of a node, compared.
bool same_node(const NodeDesc& x, const NodeDesc& y) {
  return x.stable_id == y.stable_id && same_transform(x.local, y.local) && x.name == y.name && x.props == y.props &&
         x.parent == y.parent && x.children == y.children && x.mesh == y.mesh && x.skin == y.skin &&
         same_light(x.light, y.light) && same_camera(x.camera, y.camera);
}

bool same_nodes(const std::vector<NodeDesc>& a, const std::vector<NodeDesc>& b) {
  return std::equal(a.begin(), a.end(), b.begin(), b.end(), same_node);
}

}  // namespace

void SceneDocument::checkpoint() {
  if (node_edit_active_) return;
  push_step(undo_, snapshot());
  clear_redo();
  changed_since_checkpoint_ = false;
  trim_history();
}

SceneDocument::Snapshot SceneDocument::snapshot() const {
  // (Its bytes are worked out once it is kept: see push_step.)
  Snapshot state{desc_.nodes, desc_.materials, version_, {}, 0};
  for (int i = 0; i < node_count(); ++i) {
    if (!terrain_grid(i)) continue;
    const PrimitiveDesc& ground = desc_.meshes[static_cast<std::size_t>(desc_.nodes[i].mesh)].primitives.front();
    state.shapes[desc_.nodes[i].mesh] = {ground.positions, ground.normals};
  }
  return state;
}

void SceneDocument::touch() {
  // The first change after a checkpoint makes a new state; later unrecorded changes (a drag) extend it.
  if (!changed_since_checkpoint_) {
    version_ = ++next_version_;
    changed_since_checkpoint_ = true;
  }
}

bool SceneDocument::same_content(const Snapshot& a, const Snapshot& b) {
  // Every part of a snapshot but its version, so nothing a checkpoint keeps can change unnoticed.
  return same_nodes(a.nodes, b.nodes) && same_materials(a.materials, b.materials) && a.shapes == b.shapes;
}

void SceneDocument::drop_checkpoint_if_unchanged() {
  if (undo_.empty()) return;
  if (!same_content(undo_.back(), snapshot())) return;
  version_ = undo_.back().version;
  pop_step(undo_);
  changed_since_checkpoint_ = false;
}

void SceneDocument::restore(Snapshot& state) {
  desc_.nodes = std::move(state.nodes);
  refresh_node_references();
  restore_materials(state.materials);
  restore_shapes(state.shapes);
  version_ = state.version;
  changed_since_checkpoint_ = false;
}

void SceneDocument::restore_materials(const std::vector<MaterialDesc>& snapshot) {
  // Materials are only ever appended, so a snapshot may be shorter: newer ones stay as they are.
  const std::size_t kept = std::min(snapshot.size(), desc_.materials.size());
  std::copy_n(snapshot.begin(), kept, desc_.materials.begin());
}

bool SceneDocument::undo() {
  if (node_edit_active_) return false;
  if (undo_.empty()) return false;
  push_step(redo_, snapshot());
  Snapshot state = pop_step(undo_);
  restore(state);
  return true;
}

bool SceneDocument::redo() {
  if (node_edit_active_) return false;
  if (redo_.empty()) return false;
  push_step(undo_, snapshot());
  Snapshot state = pop_step(redo_);
  restore(state);
  return true;
}

}  // namespace redgem
