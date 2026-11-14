#include "app/scene_joints.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <variant>

#include "core/log.hpp"
#include "physics/joint_config.hpp"

namespace redgem::joints {
namespace {

void warn(const GameObject& object, const std::string& warning) {
  log::warning("object '" + object.name + "': " + warning);
}

// The copy made of `end`, or `end` itself when it was not copied.
ObjectId copy_or_same(const CopyPairs& copies, ObjectId end) {
  const auto copied =
      std::find_if(copies.begin(), copies.end(), [end](const auto& pair) { return pair.first == end; });
  return copied != copies.end() ? copied->second : end;
}

// What the joint of `original` (an object the scene has, loaded or itself a copy) holds to: the
// other end of the joint made for it, or else the object its stable id names.
ObjectId held_end(const Scene& scene, const SceneJoints& made, ObjectId original, const std::string& other) {
  const auto joint = made.find(scene.handle(original));
  const ObjectId end = joint != made.end() ? scene.resolve(joint->second.other) : kNoObject;
  return end != kNoObject ? end : scene.find_by_stable_id(other);
}

// Where the ends of a joint are; none, logged, when its other end is not in the scene.
std::optional<JointEnds> ends_of(const Scene& scene, ObjectId id, ObjectId other_end, const JointConfig& config) {
  JointEnds ends{id, scene.world_transform(id), kNoObject, {}};
  if (config.other.empty()) return ends;
  if (other_end == kNoObject) {
    log::error("object '" + scene.at(id).name + "': its joint's other object (" + config.other + ") is not in the scene");
    return std::nullopt;
  }
  ends.b = other_end;
  ends.b_world = scene.world_transform(other_end);
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

// The joint an object's properties ask for, when it can be made; what is wrong is logged.
std::optional<JointConfig> usable_config(const Scene& scene, ObjectId id) {
  if (scene.is_prefab(id)) return std::nullopt;  // templates are inert
  const GameObject& object = scene.at(id);
  std::optional<JointConfig> config = joint_config_from_props(object.props);
  if (!config) return std::nullopt;
  for (const std::string& warning : config->warnings) warn(object, warning);
  return config->known_kind ? config : std::nullopt;
}

// Makes the joint of `id`, whose other end is `other_end` (kNoObject when it is not there).
void make(const Scene& scene, PhysicsWorld& physics, ObjectId id, ObjectId other_end, const JointConfig& config,
          SceneJoints& made) {
  const std::optional<JointEnds> ends = ends_of(scene, id, other_end, config);
  if (ends) add(scene, physics, joint_desc(config, *ends), made);
}

}  // namespace

void create_all(const bodies::Source& source, PhysicsWorld& physics, SceneJoints& made) {
  made.clear();
  const Scene& scene = source.scene;
  for (const ObjectId id : scene.alive_ids()) {
    const std::optional<JointConfig> config = usable_config(scene, id);
    if (config) make(scene, physics, id, scene.find_by_stable_id(config->other), *config, made);
  }
}

void create_copies(const bodies::Source& source, PhysicsWorld& physics, const CopyPairs& copies, SceneJoints& made) {
  const Scene& scene = source.scene;
  for (const auto& [original, copy] : copies) {
    const std::optional<JointConfig> config = usable_config(scene, copy);
    if (!config) continue;
    const ObjectId other_end = copy_or_same(copies, held_end(scene, made, original, config->other));
    make(scene, physics, copy, other_end, *config, made);
  }
}

}  // namespace redgem::joints
