// Live copies of a scene's objects (Scene.spawn).

#include <algorithm>

#include "scene/props.hpp"
#include "scene/scene.hpp"

namespace redgem {
namespace {

using CopyPair = std::pair<ObjectId, ObjectId>;

// A copy before it gets its place in the scene: live, on its own, following no animation ahead.
// It starts a life of its own: an original about to be destroyed hands that on to no copy.
GameObject detached_copy(const GameObject& original) {
  GameObject copy = original;
  copy.pending_destroy = false;
  copy.children.clear();
  copy.prefab = false;
  copy.props.erase(props::kPrefab);
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

ObjectId Scene::free_slot() {
  if (free_slots_.empty()) {
    objects_.emplace_back();
    return static_cast<ObjectId>(objects_.size() - 1);
  }
  const ObjectId slot = free_slots_.back();
  free_slots_.pop_back();
  return slot;
}

ObjectId Scene::add_copy(ObjectId original, ObjectId parent, ObjectId root) {
  GameObject copy = detached_copy(at(original));
  const ObjectId id = free_slot();
  copy.id = id;
  copy.serial = new_object_serial();
  copy.parent = parent;
  copy.copy_root = root == kNoObject ? id : root;
  copies_[copy.copy_of].push_back(id);
  objects_[id] = std::move(copy);
  if (parent != kNoObject) objects_[parent].children.push_back(id);
  return id;
}

std::vector<ObjectId> Scene::clone(ObjectId source) {
  if (!is_alive(source)) return {};
  const Transform world = world_transform(source);
  const std::vector<CopyPair> copies = copy_subtree(source);
  objects_[copies.front().second].local = world;  // the root keeps its world placement
  return with_copied_joints(copies);
}

std::vector<CopyPair> Scene::copy_subtree(ObjectId source) {
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
  return copies;
}

std::vector<ObjectId> Scene::with_copied_joints(const std::vector<CopyPair>& copies) {
  std::vector<ObjectId> created;
  for (const auto& [original, id] : copies) {
    for (ObjectHandle& joint : objects_[id].joints) joint = copied_joint(joint, copies);
    created.push_back(id);
  }
  return created;
}

ObjectHandle Scene::copied_joint(ObjectHandle joint, const std::vector<CopyPair>& copies) const {
  const ObjectId bone = resolve(joint);
  // A bone that is gone stays gone: its slot may hold another object by now.
  if (bone == kNoObject) return joint;
  return handle(copy_or_original(copies, bone));
}

std::vector<ObjectId> Scene::copies_of(ObjectId original) const {
  const auto found = copies_.find(original);
  return found == copies_.end() ? std::vector<ObjectId>{} : found->second;
}

}  // namespace redgem
