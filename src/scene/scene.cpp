#include "scene/scene.hpp"

#include <stdexcept>

namespace redgem {
namespace {

bool is_live_and_called(const GameObject& object, const std::string& name) {
  return !object.destroyed && object.name == name;
}

// (Prefabs are templates: looking for a tag finds what is in the game, not what it is made from.)
bool is_live_and_tagged(const GameObject& object, const std::string& tag) {
  return !object.destroyed && !object.prefab && object.tag == tag;
}

bool is_live_camera(const GameObject& object) { return !object.destroyed && object.camera.has_value(); }

// Marked for destruction and not destroyed yet.
bool awaits_destruction(const GameObject& object) { return object.pending_destroy && !object.destroyed; }

}  // namespace

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
    if (is_live_and_called(object, name)) return object.id;
  }
  return kNoObject;
}

std::vector<ObjectId> Scene::find_all_by_tag(const std::string& tag) const {
  std::vector<ObjectId> ids;
  for (const GameObject& object : objects_) {
    if (is_live_and_tagged(object, tag)) ids.push_back(object.id);
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
    if (is_live_camera(object)) return object.id;
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

void Scene::request_destroy(ObjectId id) {
  if (!is_alive(id)) return;
  GameObject& object = objects_[id];
  object.pending_destroy = true;
  for (const ObjectId child : object.children) request_destroy(child);
}

std::vector<ObjectId> Scene::pending_destroy_ids() const {
  std::vector<ObjectId> ids;
  for (const GameObject& object : objects_) {
    if (awaits_destruction(object)) ids.push_back(object.id);
  }
  return ids;
}

void Scene::finalize_destroy(ObjectId id) {
  GameObject& object = at(id);
  object.destroyed = true;
  object.pending_destroy = false;
}

}  // namespace redgem
