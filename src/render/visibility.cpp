#include "render/visibility.hpp"

#include <raymath.h>

#include <algorithm>
#include <cmath>

#include "render/render_math.hpp"

namespace redgem::render_math {
namespace {

float longest_scale(Vec3 scale) { return std::max({std::fabs(scale.x), std::fabs(scale.y), std::fabs(scale.z)}); }

// Half of a box's diagonal: the radius of the sphere around it.
float half_diagonal(const Bounds& bounds) { return length(bounds.max - bounds.min) * kHalf; }

// Where an object is in the world, from the places already worked out for the objects before
// it. (When the parent is not there yet, e.g. after a reparent, ask the scene.)
Transform placed(const Scene& scene, const GameObject& object, const std::vector<Transform>& worlds,
                 std::size_t index) {
  if (object.parent == kNoObject) return object.local;
  const auto parent = static_cast<std::size_t>(object.parent);
  return parent < index ? combine(worlds[parent], object.local) : scene.world_transform(object.id);
}

// Whether an object is one a pass may draw: alive, visible, and not a prefab (a spawn template)
// unless prefabs are shown.
bool may_be_drawn(const GameObject& object, bool show_prefabs) {
  const bool hidden_as_prefab = object.prefab && !show_prefabs;
  return !object.destroyed && object.visible && !hidden_as_prefab;
}

}  // namespace

// --- Where things are ------------------------------------------------------------------------

void world_transforms(const Scene& scene, std::vector<Transform>& worlds) {
  const std::size_t count = scene.size();
  worlds.resize(count);
  for (std::size_t i = 0; i < count; ++i) {
    const GameObject& object = scene.at(static_cast<ObjectId>(i));
    if (!object.destroyed) worlds[i] = placed(scene, object, worlds, i);
  }
}

// --- What is worth drawing -------------------------------------------------------------------

BoundingSphere world_sphere(const Bounds& bounds, const Transform& world) {
  const Vec3 center = world.position + rotate(world.rotation, hadamard(bounds.center(), world.scale));
  return {center, half_diagonal(bounds) * longest_scale(world.scale)};
}

BoundingSphere skinned_sphere(const Bounds& bounds, const Matrix& first_bone) {
  const Vector3 moved = Vector3Transform(to_raylib(bounds.center()), first_bone);
  // A scaled rig is bigger: the length of the bone's first axis says by how much.
  const float stretch = kSkinnedBoundsMargin * Vector3Length({first_bone.m0, first_bone.m1, first_bone.m2});
  return {to_vec3(moved), half_diagonal(bounds) * stretch};
}

float smallest_angle_drawn(float asked, bool depth_pass) {
  if (asked > 0.0f) return asked;
  return depth_pass ? kMinShadowAngle : kMinVisibleAngle;
}

bool has_something_to_draw(const GameObject& object, bool show_prefabs, std::size_t mesh_count) {
  return may_be_drawn(object, show_prefabs) && is_index_of(object.mesh, mesh_count);
}

bool worth_drawing(const View& view, const BoundingSphere& sphere, float from, float to) {
  if (!sphere_in_frustum(view.frustum, sphere.center, sphere.radius)) return false;
  if (too_small_to_see(sphere.center, sphere.radius, view.viewer, view.min_angle)) return false;
  return !outside_distance_range(sphere.center, view.viewer, from, to);
}

Sight sight_of(const Bounds& bounds, const Transform& world) {
  if (!bounds.valid) return {{world.position, 0.0f}, false};
  return {world_sphere(bounds, world), true};
}

Sight skinned_sight_of(const Bounds& bounds, const Transform& world, const Matrix& first_bone) {
  if (!bounds.valid) return sight_of(bounds, world);
  return {skinned_sphere(bounds, first_bone), true};
}

bool sighted(const View& view, const Sight& sight, float from, float to) {
  if (!sight.bounded) return !outside_distance_range(sight.sphere.center, view.viewer, from, to);
  return worth_drawing(view, sight.sphere, from, to);
}

}  // namespace redgem::render_math
