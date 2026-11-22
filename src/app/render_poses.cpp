#include "app/render_poses.hpp"

#include <algorithm>

#include "core/math.hpp"
#include "scene/transform.hpp"

namespace redgem {
namespace {

// How far (meters, and quaternion parts) an object may be from where its step left it and still
// count as left there: what a float's rounding moves it, far less than anything a script does.
constexpr float kUnmovedTolerance = 1e-4f;
// The same for a rotation, part by part: about 0.001 degrees. (A dot product near 1 cannot tell
// turns under about a degree apart, in floats.)
constexpr float kUnturnedTolerance = 1e-5f;

bool parts_within(Quat a, Quat b, float tolerance) {
  return approx_equal(a.x, b.x, tolerance) && approx_equal(a.y, b.y, tolerance) && approx_equal(a.z, b.z, tolerance) &&
         approx_equal(a.w, b.w, tolerance);
}

// Whether two rotations are the same turn (q and -q are).
bool same_turn(Quat a, Quat b) {
  return parts_within(a, b, kUnturnedTolerance) || parts_within(a, Quat{-b.x, -b.y, -b.z, -b.w}, kUnturnedTolerance);
}

// How many objects up the object hangs from.
int depth_of(const Scene& scene, ObjectId id) {
  int depth = 0;
  for (ObjectId at = scene.at(id).parent; at != kNoObject; at = scene.at(at).parent) ++depth;
  return depth;
}

bool moves_on_its_own(BodyType type) {
  return type == BodyType::Dynamic || type == BodyType::Character || type == BodyType::Vehicle;
}

bool interpolated(const PhysicsWorld& physics, ObjectId id) {
  return physics.has_object(id) && moves_on_its_own(physics.body_type(id));
}

}  // namespace

void RenderPoses::begin_step(const Scene& scene, const PhysicsWorld& physics) {
  clear();
  for (const ObjectId id : scene.alive_ids()) {
    if (!interpolated(physics, id)) continue;
    const Transform now = scene.world_transform(id);
    index_[id] = steps_.size();
    steps_.push_back({id, now, now, physics.body_type(id) != BodyType::Character});
  }
}

void RenderPoses::end_step(const Scene& scene) {
  for (Steps& steps : steps_) {
    if (scene.is_alive(steps.id)) steps.after = scene.world_transform(steps.id);
  }
}

void RenderPoses::clear() {
  steps_.clear();
  index_.clear();
}

bool RenderPoses::unmoved(const Scene& scene, const Steps& steps) const {
  if (!scene.is_alive(steps.id)) return false;
  const Transform now = scene.world_transform(steps.id);
  const bool same_place = approx_equal(now.position, steps.after.position, kUnmovedTolerance);
  return same_place && (!steps.turns || same_turn(now.rotation, steps.after.rotation));
}

Transform RenderPoses::between(const Steps& steps, const Transform& now, float share) {
  Transform drawn = now;
  drawn.position = mix(steps.before.position, steps.after.position, share);
  if (steps.turns) drawn.rotation = mix(steps.before.rotation, steps.after.rotation, share);
  return drawn;
}

std::vector<RenderPoses::Placement> RenderPoses::planned(const Scene& scene, float share) const {
  std::vector<Placement> placements;
  for (const Steps& steps : steps_) {
    if (!unmoved(scene, steps)) continue;
    const Transform drawn = between(steps, scene.world_transform(steps.id), std::max(share, 0.0f));
    placements.push_back({depth_of(scene, steps.id), steps.id, drawn});
  }
  // (Each where it is drawn in the world: a parent first, so that placing it moves nothing placed.)
  std::stable_sort(placements.begin(), placements.end(),
                   [](const Placement& a, const Placement& b) { return a.depth < b.depth; });
  return placements;
}

void RenderPoses::place(Scene& scene, float share) {
  kept_.clear();
  if (share >= 1.0f) return;
  for (const Placement& placement : planned(scene, share)) {
    kept_.emplace_back(placement.id, scene.at(placement.id).local);
    scene.set_world_transform(placement.id, placement.drawn);
  }
}

void RenderPoses::restore(Scene& scene) {
  // (Last placed first: an object placed on another placed one gets back the local it had.)
  for (auto kept = kept_.rbegin(); kept != kept_.rend(); ++kept) scene.at(kept->first).local = kept->second;
  kept_.clear();
}

const RenderPoses::Steps* RenderPoses::carrier(const Scene& scene, ObjectId id) const {
  for (ObjectId at = id; at != kNoObject; at = scene.at(at).parent) {
    const auto found = index_.find(at);
    if (found != index_.end() && unmoved(scene, steps_[found->second])) return &steps_[found->second];
  }
  return nullptr;
}

Transform RenderPoses::drawn(const Scene& scene, ObjectId id, float share) const {
  const Transform world = scene.world_transform(id);
  const Steps* steps = carrier(scene, id);
  if (steps == nullptr || share >= 1.0f) return world;
  const Transform carried = scene.world_transform(steps->id);
  const Transform placed = between(*steps, carried, std::max(share, 0.0f));
  return combine(placed, relative_to(carried, world));
}

}  // namespace redgem
