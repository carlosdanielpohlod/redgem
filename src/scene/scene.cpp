#include "scene/scene.hpp"

#include <algorithm>
#include <atomic>
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

void erase_id(std::vector<ObjectId>& ids, ObjectId id) {
  ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end());
}

}  // namespace

ObjectSerial new_object_serial() {
  // For the whole process, not one scene: a reference kept across a scene switch must not match.
  static std::atomic<ObjectSerial> last{kNoSerial};
  return ++last;
}

bool Scene::is_alive(ObjectId id) const { return valid(id) && !objects_[id].destroyed; }

ObjectHandle Scene::handle(ObjectId id) const {
  return is_alive(id) ? ObjectHandle{id, objects_[id].serial} : kNoHandle;
}

ObjectId Scene::resolve(ObjectHandle handle) const {
  const bool same_object = is_alive(handle.id) && objects_[handle.id].serial == handle.serial;
  return same_object ? handle.id : kNoObject;
}

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
  // Walked from the top of each pending subtree: a copy's child may sit in a lower slot than it.
  std::vector<ObjectId> ids;
  for (const GameObject& object : objects_) {
    if (awaits_destruction(object) && !parent_is_pending(object)) append_pending(object.id, ids);
  }
  return ids;
}

void Scene::append_pending(ObjectId id, std::vector<ObjectId>& out) const {
  out.push_back(id);
  for (const ObjectId child : objects_[id].children) {
    if (awaits_destruction(objects_[child])) append_pending(child, out);
  }
}

bool Scene::parent_is_pending(const GameObject& object) const {
  return object.parent != kNoObject && awaits_destruction(objects_[object.parent]);
}

void Scene::finalize_destroy(ObjectId id) {
  GameObject& object = at(id);
  object.destroyed = true;
  object.pending_destroy = false;
  leave_parent(object);
  leave_copies(object);
  if (!reusable(object)) return;
  objects_[id] = GameObject{};  // nothing of the old object lingers in the slot
  objects_[id].id = id;
  objects_[id].destroyed = true;
  free_slots_.push_back(id);
}

void Scene::leave_parent(const GameObject& object) {
  if (valid(object.parent)) erase_id(objects_[object.parent].children, object.id);
}

void Scene::leave_copies(const GameObject& object) {
  const auto found = copies_.find(object.copy_of);
  if (found == copies_.end()) return;
  erase_id(found->second, object.id);
  if (found->second.empty()) copies_.erase(found);
}

bool Scene::reusable(const GameObject& object) const {
  // (Children leave first: a parent still listing some is destroyed before them, which the engine
  // never does, and its slot is kept so they do not end up under a stranger. A skinned object
  // using it as a bone holds a handle, which finds nothing once it is gone.)
  const bool made_while_running = static_cast<std::size_t>(object.id) >= loaded_count_;
  return made_while_running && object.children.empty();
}

}  // namespace redgem
