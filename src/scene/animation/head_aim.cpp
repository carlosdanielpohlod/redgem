#include "scene/animation/head_aim.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace redgem {
namespace {

constexpr float kRotationTolerance = 1e-5f;  // a local rotation this close to what was written is it
constexpr float kFullWeight = 1.0f;
constexpr float kNoWeight = 0.0f;

// Whether two directions point the same way or opposite ways: no axis turns one into the other.
bool parallel(Vec3 turn_axis) { return length(turn_axis) <= kEpsilon; }

}  // namespace

Quat aimed_rotation(Quat world, Vec3 axis, Vec3 toward, float most_radians, float weight) {
  const Vec3 facing = normalize(rotate(world, axis));
  const Vec3 wanted = normalize(toward);
  const Vec3 turn_axis = cross(facing, wanted);
  if (parallel(turn_axis)) return world;
  const float angle = std::acos(std::clamp(dot(facing, wanted), -kFullWeight, kFullWeight));
  const float turn = std::min(angle, most_radians) * weight;
  return normalize(quat_from_axis_angle(turn_axis, turn) * world);
}

void HeadAims::aim(const Scene& scene, ObjectId character, ObjectId bone, Vec3 target, float most_degrees) {
  Aim& aim = aims_[scene.handle(bone)];
  const bool fresh = !aim.turned && aim.weight <= kNoWeight;
  if (fresh) {
    const Quat bone_world = scene.world_transform(bone).rotation;
    aim.axis = normalize(rotate(conjugate(bone_world), scene.world_transform(character).forward()));
  }
  aim.target = target;
  aim.most_radians = degrees_to_radians(most_degrees);
  aim.held = true;
}

void HeadAims::release(const Scene& scene, ObjectId bone) {
  const auto found = aims_.find(scene.handle(bone));
  if (found != aims_.end()) found->second.held = false;
}

bool HeadAims::aiming(const Scene& scene, ObjectId bone) const { return aims_.count(scene.handle(bone)) != 0; }

void HeadAims::apply(Scene& scene, float dt) {
  for (auto entry = aims_.begin(); entry != aims_.end();) {
    const ObjectId bone = scene.resolve(entry->first);
    const bool kept = bone != kNoObject && turn(scene, bone, entry->second, dt);
    entry = kept ? std::next(entry) : aims_.erase(entry);
  }
}

bool HeadAims::turn(Scene& scene, ObjectId bone, Aim& aim, float dt) {
  aim.pose = pose_of(scene.at(bone), aim);
  scene.at(bone).local.rotation = aim.pose;
  aim.weight = eased(aim, dt);
  if (!aim.held && aim.weight <= kNoWeight) return false;
  const Transform world = scene.world_transform(bone);
  const Vec3 toward = aim.target - world.position;
  scene.set_world_rotation(bone, aimed_rotation(world.rotation, aim.axis, toward, aim.most_radians, aim.weight));
  aim.written = scene.at(bone).local.rotation;
  aim.turned = true;
  return true;
}

Quat HeadAims::pose_of(const GameObject& bone, const Aim& aim) {
  const bool untouched = aim.turned && approx_equal(bone.local.rotation, aim.written, kRotationTolerance);
  return untouched ? aim.pose : bone.local.rotation;
}

float HeadAims::eased(const Aim& aim, float dt) {
  const float step = dt / kEaseSeconds;
  const float goal = aim.held ? kFullWeight : kNoWeight;
  return aim.weight < goal ? std::min(goal, aim.weight + step) : std::max(goal, aim.weight - step);
}

}  // namespace redgem
