#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "scene/property.hpp"
#include "scene/scatter.hpp"
#include "scene/scene_desc.hpp"
#include "scene/transform.hpp"

namespace redgem {

using ObjectId = int;
inline constexpr ObjectId kNoObject = kNoIndex;
// Which object a slot holds: no two objects ever get the same serial, in any scene of the session,
// so a reference that kept one can tell when its object is gone even if the slot holds another.
using ObjectSerial = std::uint64_t;
inline constexpr ObjectSerial kNoSerial = 0;
// A serial no object had before.
ObjectSerial new_object_serial();

// How a part of the engine keeps an object it does not own, to find it again later (a bone, a
// wheel, a character's own clip, a behaviour's object): Scene::resolve finds that object only,
// and nothing once it is destroyed, even when its slot holds a new object by then.
// The rule: what keeps an object past the step it was handed it keeps a handle. Only a registry
// the engine itself updates in the step an object is destroyed may key by bare id (the physics
// bodies, and the scene's own hierarchy and copies).
struct ObjectHandle {
  ObjectId id = kNoObject;
  ObjectSerial serial = kNoSerial;

  bool operator==(const ObjectHandle& other) const { return id == other.id && serial == other.serial; }
  bool operator!=(const ObjectHandle& other) const { return !(*this == other); }
  bool operator<(const ObjectHandle& other) const { return serial < other.serial; }  // serials never repeat
};
inline constexpr ObjectHandle kNoHandle{};

struct GameObject {
  ObjectId id = kNoObject;
  ObjectSerial serial = kNoSerial;
  std::string name;
  std::string tag;
  ObjectId parent = kNoObject;
  std::vector<ObjectId> children;
  Transform local;
  int mesh = kNoIndex;
  int skin = kNoIndex;  // skeleton deforming the mesh (index into the scene description's skins)
  // The objects acting as that skeleton's joints, in the skin's order. Copies made by clone()
  // point at their own copies of the bones.
  std::vector<ObjectHandle> joints;
  std::optional<CameraDesc> camera;
  std::optional<LightDesc> light;
  PropertyMap props;
  bool visible = true;
  Vec3 tint = Vec3::one();  // multiplies the colors of this object's mesh (prop `tint = "#rrggbb"`)
  // Drawn only while this far from the camera, in meters (props `visible_from`, `visible_to`).
  float visible_from = 0.0f;
  float visible_to = std::numeric_limits<float>::infinity();
  bool prefab = false;      // a template for Scene.spawn: not simulated, not drawn
  bool cast_shadow = true;  // in the sun's shadows (prop `cast_shadow`)
  // Whether wind sways its foliage (prop `sway`; unless said, it does when no skeleton moves it).
  bool sways = true;
  // For objects made by clone(): the object of the loaded scene this one copies (animations of
  // the original also move its copies) and the root of the copy it belongs to.
  ObjectId copy_of = kNoObject;
  ObjectId copy_root = kNoObject;
  // On the root of a copy: seconds its animations run ahead of the original's, so a crowd of
  // copies does not move in step.
  float animation_offset = 0.0f;
  bool destroyed = false;
  bool pending_destroy = false;
};

// Runtime object hierarchy. An id is a slot. The slots of the loaded scene's objects are theirs for
// good (id == node index); the slot of an object made while the game runs (a copy) is used again
// once that object is destroyed, so a game that keeps spawning does not keep growing. What tells
// two objects of one slot apart is their serial (see ObjectSerial).
// Implemented in parts: scene.cpp (finding, placing and destroying objects), scene_from_desc.cpp
// (the objects of a loaded file) and scene_copies.cpp (live copies).
class Scene {
 public:
  static Scene from_desc(const SceneDesc& desc);
  // Makes one object of a from_desc scene again from its node, as from_desc would, keeping its
  // serial: for an edit to the node itself (where it is, its light, its camera). The node's place
  // in the hierarchy and its being a prefab must not have changed; for that, make the scene again.
  void refresh_from_desc(const SceneDesc& desc, ObjectId id);

  std::size_t size() const { return objects_.size(); }
  bool is_alive(ObjectId id) const;
  // A handle to an alive object (kNoHandle for any other id), and the id of the object a handle
  // was made for while it is still alive (kNoObject once it is destroyed).
  ObjectHandle handle(ObjectId id) const;
  ObjectId resolve(ObjectHandle handle) const;
  GameObject& at(ObjectId id);
  const GameObject& at(ObjectId id) const;

  ObjectId find(const std::string& name) const;
  std::vector<ObjectId> find_all_by_tag(const std::string& tag) const;
  std::vector<ObjectId> alive_ids() const;
  // First alive object holding a camera, or kNoObject.
  ObjectId main_camera() const;

  Transform world_transform(ObjectId id) const;
  void set_world_transform(ObjectId id, const Transform& world);
  void set_world_position(ObjectId id, Vec3 position);
  void set_world_rotation(ObjectId id, Quat rotation);

  // True for objects marked `prefab = true` and their descendants.
  bool is_prefab(ObjectId id) const { return valid(id) && objects_[id].prefab; }
  // Copies `source` and its descendants as live (non-prefab) objects at the root of the scene.
  // Returns the ids of the copies, the copy of `source` first; none for a source that is not
  // alive. Copies are new objects: one made of an object pending destruction lives on.
  std::vector<ObjectId> clone(ObjectId source);
  // The alive copies of an object of the loaded scene (see GameObject::copy_of).
  std::vector<ObjectId> copies_of(ObjectId original) const;

  // Marks the object and all its descendants for destruction at the end of the step.
  void request_destroy(ObjectId id);
  // Objects marked for destruction but still alive (their on_destroy can still use them), every
  // parent ahead of its children.
  std::vector<ObjectId> pending_destroy_ids() const;
  // Completes the destruction of a pending object (after its on_destroy ran): it leaves its parent
  // and the copies of its original, and its slot may then hold a new object.
  void finalize_destroy(ObjectId id);

  // Copies of meshes in great numbers that are not objects (Scatter: see scatter.hpp).
  Scattered& scattered() { return scattered_; }
  const Scattered& scattered() const { return scattered_; }

 private:
  // Adds the copy of one object under `parent` (kNoObject for the root of a copy, whose root is
  // then itself). Returns its id.
  ObjectId add_copy(ObjectId original, ObjectId parent, ObjectId root);
  // Copies `source` and what is under it, parents first: (original, its copy) pairs, the copy of
  // `source` first.
  std::vector<std::pair<ObjectId, ObjectId>> copy_subtree(ObjectId source);
  // A copied character is deformed by its own copied bones (bones outside the copy stay shared).
  // Takes (original, its copy) pairs and returns the copies' ids, in that order.
  std::vector<ObjectId> with_copied_joints(const std::vector<std::pair<ObjectId, ObjectId>>& copies);
  // What a copied bone handle becomes: the copy of a bone copied with it, the same bone when it
  // lies outside the copy, and unchanged (finding nothing) when the bone is gone.
  ObjectHandle copied_joint(ObjectHandle joint, const std::vector<std::pair<ObjectId, ObjectId>>& copies) const;
  bool valid(ObjectId id) const { return is_index_of(id, objects_.size()); }
  // A slot for a new copy: one a destroyed copy left, else a new one at the end.
  ObjectId free_slot();
  // The handles of the bones of a node's skin, which must all be in the scene by then.
  std::vector<ObjectHandle> joints_of(const NodeDesc& node, const SceneDesc& desc) const;
  // Puts the pending objects of `id`'s subtree in `out`, parents first.
  void append_pending(ObjectId id, std::vector<ObjectId>& out) const;
  // Whether a pending object's parent is pending too (so the parent lists it).
  bool parent_is_pending(const GameObject& object) const;
  void leave_parent(const GameObject& object);
  void leave_copies(const GameObject& object);
  // Whether a destroyed object's slot may hold a new object: one made while the game ran.
  bool reusable(const GameObject& object) const;

  std::vector<GameObject> objects_;
  std::size_t loaded_count_ = 0;      // slots of the loaded scene's objects, never used again
  std::vector<ObjectId> free_slots_;  // left by destroyed copies, used again last in first out
  std::unordered_map<ObjectId, std::vector<ObjectId>> copies_;  // original -> its copies not finalized
  Scattered scattered_;
};

}  // namespace redgem
