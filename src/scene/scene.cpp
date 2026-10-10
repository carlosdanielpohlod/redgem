#include "scene/scene.hpp"

#include <algorithm>

#include <stdexcept>

#include "core/config.hpp"

namespace redgem {

Scene Scene::from_desc(const SceneDesc& desc) {
  Scene scene;
  scene.objects_.reserve(desc.nodes.size());
  for (std::size_t i = 0; i < desc.nodes.size(); ++i) {
    const NodeDesc& node = desc.nodes[i];
    GameObject object;
    object.id = static_cast<ObjectId>(i);
    object.name = node.name;
    object.parent = node.parent;
    object.children = node.children;
    object.local = node.local;
    object.mesh = node.mesh;
    object.skin = node.skin;
    if (node.skin >= 0 && static_cast<std::size_t>(node.skin) < desc.skins.size()) {
      object.joints = desc.skins[static_cast<std::size_t>(node.skin)].joints;
    }
    object.camera = node.camera;
    object.light = node.light;
    object.props = node.props;
    object.tag = get_string(node.props, config::kPropTag).value_or("");
    object.visible = get_bool(node.props, config::kPropVisible).value_or(true);
    if (const auto tint = get_string(node.props, config::kPropTint)) {
      object.tint = parse_hex_color(*tint).value_or(Vec3::one());
    }
    // Parents come first, so a prefab's descendants inherit the flag.
    object.prefab = get_bool(node.props, config::kPropPrefab).value_or(false) ||
                    (node.parent != kNoIndex && scene.objects_[node.parent].prefab);
    scene.objects_.push_back(std::move(object));
  }
  return scene;
}

bool Scene::is_alive(ObjectId id) const { return valid(id) && !objects_[id].destroyed; }

GameObject& Scene::at(ObjectId id) {
  if (!valid(id)) throw std::out_of_range("invalid object id " + std::to_string(id));
  return objects_[id];
}

const GameObject& Scene::at(ObjectId id) const {
  if (!valid(id)) throw std::out_of_range("invalid object id " + std::to_string(id));
  return objects_[id];
}

ObjectId Scene::find(const std::string& name) const {
  for (const GameObject& object : objects_) {
    if (!object.destroyed && object.name == name) return object.id;
  }
  return kNoObject;
}

std::vector<ObjectId> Scene::find_all_by_tag(const std::string& tag) const {
  std::vector<ObjectId> ids;
  for (const GameObject& object : objects_) {
    if (!object.destroyed && !object.prefab && object.tag == tag) ids.push_back(object.id);
  }
  return ids;
}

std::vector<ObjectId> Scene::alive_ids() const {
  std::vector<ObjectId> ids;
  for (const GameObject& object : objects_) {
    if (!object.destroyed) ids.push_back(object.id);
  }
  return ids;
}

ObjectId Scene::main_camera() const {
  for (const GameObject& object : objects_) {
    if (!object.destroyed && object.camera.has_value()) return object.id;
  }
  return kNoObject;
}

Transform Scene::world_transform(ObjectId id) const {
  const GameObject& object = at(id);
  if (object.parent == kNoObject) return object.local;
  return combine(world_transform(object.parent), object.local);
}

void Scene::set_world_transform(ObjectId id, const Transform& world) {
  GameObject& object = at(id);
  object.local = object.parent == kNoObject ? world : relative_to(world_transform(object.parent), world);
}

void Scene::set_world_position(ObjectId id, Vec3 position) {
  Transform world = world_transform(id);
  world.position = position;
  set_world_transform(id, world);
}

void Scene::set_world_rotation(ObjectId id, Quat rotation) {
  Transform world = world_transform(id);
  world.rotation = rotation;
  set_world_transform(id, world);
}

std::vector<ObjectId> Scene::clone(ObjectId source) {
  std::vector<ObjectId> created;
  std::vector<std::pair<ObjectId, ObjectId>> copies;  // (original, its copy)
  const Transform world = world_transform(source);
  // Breadth-first over the subtree; the new parent of each copy is known when it is created.
  std::vector<std::pair<ObjectId, ObjectId>> queue = {{source, kNoObject}};  // (original, copy's parent)
  for (std::size_t i = 0; i < queue.size(); ++i) {
    const auto [original, parent] = queue[i];
    GameObject copy = at(original);
    copy.id = static_cast<ObjectId>(objects_.size());
    copy.parent = parent;
    copy.children.clear();
    copy.prefab = false;
    copy.props.erase(config::kPropPrefab);
    // A copy of a copy still follows the animations of the first original.
    copy.copy_of = copy.copy_of == kNoObject ? original : copy.copy_of;
    copy.copy_root = created.empty() ? copy.id : created.front();
    copy.animation_offset = 0.0f;
    if (parent == kNoObject) copy.local = world;  // the root keeps its world placement
    const std::vector<ObjectId> children = at(original).children;
    objects_.push_back(std::move(copy));
    const ObjectId id = objects_.back().id;
    if (parent != kNoObject) objects_[parent].children.push_back(id);
    created.push_back(id);
    copies.push_back({original, id});
    copies_[objects_[id].copy_of].push_back(id);
    for (const ObjectId child : children) {
      if (!objects_[child].destroyed) queue.push_back({child, id});
    }
  }
  // A copied character is deformed by its own copied bones (bones outside the copy stay shared).
  for (const ObjectId id : created) {
    for (ObjectId& joint : objects_[id].joints) {
      const auto copied = std::find_if(copies.begin(), copies.end(), [joint](const auto& pair) { return pair.first == joint; });
      if (copied != copies.end()) joint = copied->second;
    }
  }
  return created;
}

std::vector<ObjectId> Scene::copies_of(ObjectId original) const {
  std::vector<ObjectId> alive;
  const auto found = copies_.find(original);
  if (found == copies_.end()) return alive;
  for (const ObjectId id : found->second) {
    if (!objects_[id].destroyed) alive.push_back(id);
  }
  return alive;
}

void Scene::request_destroy(ObjectId id) {
  if (!is_alive(id)) return;
  GameObject& object = objects_[id];
  object.pending_destroy = true;
  for (const ObjectId child : object.children) request_destroy(child);
}

std::vector<ObjectId> Scene::pending_destroy_ids() const {
  std::vector<ObjectId> ids;
  for (const GameObject& object : objects_) {
    if (object.pending_destroy && !object.destroyed) ids.push_back(object.id);
  }
  return ids;
}

void Scene::finalize_destroy(ObjectId id) {
  GameObject& object = at(id);
  object.destroyed = true;
  object.pending_destroy = false;
}

}  // namespace redgem
