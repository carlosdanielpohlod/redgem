#include "app/scene_joints.hpp"

#include <optional>
#include <variant>

#include "core/log.hpp"
#include "physics/joint_config.hpp"

namespace redgem::joints {
namespace {

// The copy of `original` made as part of the copy whose root is `root`; `original` without one.
ObjectId copy_in(const Scene& scene, ObjectId original, ObjectId root) {
  for (const ObjectId copy : scene.copies_of(original)) {
    if (scene.is_alive(copy) && scene.at(copy).copy_root == root) return copy;
  }
  return original;
}

void warn(const GameObject& object, const std::string& warning) {
  log::warning("object '" + object.name + "': " + warning);
}

// Where the ends of an object's joint are; none, logged, when its other end is not in the scene.
std::optional<JointEnds> ends_of(const Scene& scene, ObjectId id, const JointConfig& config) {
  JointEnds ends;
  ends.a = id;
  ends.a_world = scene.world_transform(id);
  if (config.other.empty()) return ends;
  ends.b = partner_of(scene, id, config.other);
  if (ends.b == kNoObject) {
    log::error("object '" + scene.at(id).name + "': its joint's other object (" + config.other + ") is not in the scene");
    return std::nullopt;
  }
  ends.b_world = scene.world_transform(ends.b);
  return ends;
}

void add(const Scene& scene, PhysicsWorld& physics, const JointDesc& desc, SceneJoints& made) {
  const std::variant<JointId, std::string> joint = physics.add_joint(desc);
  if (const auto* problem = std::get_if<std::string>(&joint)) {
    log::error("object '" + scene.at(desc.a).name + "': " + *problem);
    return;
  }
  const ObjectHandle other = desc.b == kNoObject ? kNoHandle : scene.handle(desc.b);
  made[scene.handle(desc.a)] = SceneJoint{std::get<JointId>(joint), desc.kind, other};
}

}  // namespace

ObjectId partner_of(const Scene& scene, ObjectId id, const std::string& other) {
  const ObjectId original = scene.find_by_stable_id(other);
  const ObjectId root = scene.at(id).copy_root;
  if (original == kNoObject || root == kNoObject) return original;
  return copy_in(scene, original, root);
}

void create_all(const bodies::Source& source, PhysicsWorld& physics, SceneJoints& made) {
  made.clear();
  for (const ObjectId id : source.scene.alive_ids()) create(source, physics, id, made);
}

void create(const bodies::Source& source, PhysicsWorld& physics, ObjectId id, SceneJoints& made) {
  if (source.scene.is_prefab(id)) return;  // templates are inert
  const GameObject& object = source.scene.at(id);
  const std::optional<JointConfig> config = joint_config_from_props(object.props);
  if (!config) return;
  for (const std::string& warning : config->warnings) warn(object, warning);
  if (!config->known_kind) return;
  const std::optional<JointEnds> ends = ends_of(source.scene, id, *config);
  if (ends) add(source.scene, physics, joint_desc(*config, *ends), made);
}

}  // namespace redgem::joints
