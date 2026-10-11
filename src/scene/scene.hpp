#pragma once

#include <optional>
#include <string>
#include <limits>
#include <unordered_map>
#include <vector>

#include "scene/property.hpp"
#include "scene/scene_desc.hpp"
#include "scene/transform.hpp"

namespace redgem {

using ObjectId = int;
inline constexpr ObjectId kNoObject = kNoIndex;

struct GameObject {
  ObjectId id = kNoObject;
  std::string name;
  std::string tag;
  ObjectId parent = kNoObject;
  std::vector<ObjectId> children;
  Transform local;
  int mesh = kNoIndex;
  int skin = kNoIndex;  // skeleton deforming the mesh (index into the scene description's skins)
  // The objects acting as that skeleton's joints, in the skin's order. Copies made by clone()
  // point at their own copies of the bones.
  std::vector<ObjectId> joints;
  std::optional<CameraDesc> camera;
  std::optional<LightDesc> light;
  PropertyMap props;
  bool visible = true;
  Vec3 tint = Vec3::one();  // multiplies the colors of this object's mesh (prop `tint = "#rrggbb"`)
  // Drawn only while this far from the camera, in meters (props `visible_from`, `visible_to`).
  float visible_from = 0.0f;
  float visible_to = std::numeric_limits<float>::infinity();
  bool prefab = false;  // a template for Scene.spawn: not simulated, not drawn
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

// Runtime object hierarchy. Ids are stable for the whole session (destroyed slots stay reserved).
class Scene {
 public:
  static Scene from_desc(const SceneDesc& desc);

  std::size_t size() const { return objects_.size(); }
  bool is_alive(ObjectId id) const;
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
  // Returns the ids of the copies, the copy of `source` first.
  std::vector<ObjectId> clone(ObjectId source);
  // The alive copies of an object of the loaded scene (see GameObject::copy_of).
  std::vector<ObjectId> copies_of(ObjectId original) const;

  // Marks the object and all its descendants for destruction at the end of the step.
  void request_destroy(ObjectId id);
  // Objects marked for destruction but still alive (their on_destroy can still use them).
  std::vector<ObjectId> pending_destroy_ids() const;
  // Completes the destruction of a pending object (after its on_destroy ran).
  void finalize_destroy(ObjectId id);

 private:
  bool valid(ObjectId id) const { return id >= 0 && static_cast<std::size_t>(id) < objects_.size(); }

  std::vector<GameObject> objects_;
  std::unordered_map<ObjectId, std::vector<ObjectId>> copies_;  // original -> every copy ever made
};

}  // namespace redgem
