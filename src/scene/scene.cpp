#include "scene/scene.hpp"

#include <algorithm>

#include <stdexcept>

#include "core/config.hpp"

namespace redgem {

namespace {

using CopyPair = std::pair<ObjectId, ObjectId>;

// Where a node is in the tree.
GameObject placed_object(const NodeDesc& node, ObjectId id) {
  GameObject object;
  object.id = id;
  object.name = node.name;
  object.parent = node.parent;
  object.children = node.children;
  object.local = node.local;
  return object;
}

// What a node shows: its mesh and skeleton, its camera, its light.
void copy_parts(const NodeDesc& node, const SceneDesc& desc, GameObject& object) {
  object.mesh = node.mesh;
  object.skin = node.skin;
  if (node.skin >= 0 && static_cast<std::size_t>(node.skin) < desc.skins.size()) {
    object.joints = desc.skins[static_cast<std::size_t>(node.skin)].joints;
  }
  object.camera = node.camera;
  object.light = node.light;
}

// What a node's custom properties say about the object itself.
void apply_props(const PropertyMap& props, GameObject& object) {
  object.props = props;
  object.tag = get_string(props, config::kPropTag).value_or("");
  object.visible = get_bool(props, config::kPropVisible).value_or(true);
  object.prefab = get_bool(props, config::kPropPrefab).value_or(false);
  if (const auto tint = get_string(props, config::kPropTint)) {
    object.tint = parse_hex_color(*tint).value_or(Vec3::one());
  }
  if (const auto from = get_number(props, config::kPropVisibleFrom)) object.visible_from = static_cast<float>(*from);
  if (const auto to = get_number(props, config::kPropVisibleTo)) object.visible_to = static_cast<float>(*to);
}

// A copy before it gets its place in the scene: live, on its own, following no animation ahead.
GameObject detached_copy(const GameObject& original) {
  GameObject copy = original;
  copy.children.clear();
  copy.prefab = false;
  copy.props.erase(config::kPropPrefab);
  // A copy of a copy still follows the animations of the first original.
  copy.copy_of = original.copy_of == kNoObject ? original.id : original.copy_of;
  copy.animation_offset = 0.0f;
  return copy;
}

// The copy made of `original`, or `original` itself when it is outside what was copied.
ObjectId copy_or_original(const std::vector<CopyPair>& copies, ObjectId original) {
  const auto copied =
      std::find_if(copies.begin(), copies.end(), [original](const CopyPair& pair) { return pair.first == original; });
  return copied != copies.end() ? copied->second : original;
}

}  // namespace

Scene Scene::from_desc(const SceneDesc& desc) {
  Scene scene;
  scene.objects_.reserve(desc.nodes.size());
  for (std::size_t i = 0; i < desc.nodes.size(); ++i) {
    const NodeDesc& node = desc.nodes[i];
    GameObject object = placed_object(node, static_cast<ObjectId>(i));
    copy_parts(node, desc, object);
    apply_props(node.props, object);
    // Parents come first, so a prefab's descendants inherit the flag.
    object.prefab = object.prefab || (node.parent != kNoIndex && scene.objects_[node.parent].prefab);
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

ObjectId Scene::add_copy(ObjectId original, ObjectId parent, ObjectId root) {
  GameObject copy = detached_copy(at(original));
  copy.id = static_cast<ObjectId>(objects_.size());
  copy.parent = parent;
  copy.copy_root = root == kNoObject ? copy.id : root;
  const ObjectId id = copy.id;
  copies_[copy.copy_of].push_back(id);
  objects_.push_back(std::move(copy));
  if (parent != kNoObject) objects_[parent].children.push_back(id);
  return id;
}

std::vector<ObjectId> Scene::clone(ObjectId source) {
  const Transform world = world_transform(source);
  std::vector<CopyPair> copies;  // (original, its copy)
  // Breadth-first over the subtree; the new parent of each copy is known when it is created.
  std::vector<CopyPair> queue = {{source, kNoObject}};  // (original, copy's parent)
  for (std::size_t i = 0; i < queue.size(); ++i) {
    const auto [original, parent] = queue[i];
    const ObjectId id = add_copy(original, parent, copies.empty() ? kNoObject : copies.front().second);
    copies.push_back({original, id});
    for (const ObjectId child : objects_[original].children) {
      if (!objects_[child].destroyed) queue.push_back({child, id});
    }
  }
  objects_[copies.front().second].local = world;  // the root keeps its world placement
  return with_copied_joints(copies);
}

std::vector<ObjectId> Scene::with_copied_joints(const std::vector<CopyPair>& copies) {
  std::vector<ObjectId> created;
  for (const auto& [original, id] : copies) {
    for (ObjectId& joint : objects_[id].joints) joint = copy_or_original(copies, joint);
    created.push_back(id);
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
