#pragma once

// What a view of the scene is worth drawing: where every object is, the sphere around each mesh,
// and whether that sphere is in view, big enough and at its distance. No window and no graphics
// card needed, so it can be tested.

#include <raylib.h>

#include <cstddef>
#include <vector>

#include "core/math.hpp"
#include "render/culling.hpp"
#include "scene/scene.hpp"
#include "scene/scene_desc.hpp"
#include "scene/transform.hpp"

namespace redgem::render_math {

// Parts that look smaller than this (radius over distance from the camera, radians) are skipped:
// a car's bolts at 50 m. Shadows of small things disappear sooner.
inline constexpr float kMinVisibleAngle = 0.0025f;
inline constexpr float kMinShadowAngle = 0.008f;
inline constexpr float kSkinnedBoundsMargin = 1.5f;  // animated limbs reach beyond the bind pose

// Where every object is in the world, by object id (destroyed objects keep what they had).
// Parents are created before their children, so a parent's place is reused instead of walking
// each object's chain of parents again.
void world_transforms(const Scene& scene, std::vector<Transform>& worlds);

struct BoundingSphere {
  Vec3 center;
  float radius = 0.0f;
};
// The sphere around a mesh placed in the world.
BoundingSphere world_sphere(const Bounds& bounds, const Transform& world);
// A skinned mesh is placed by its skeleton, not by its own node: the sphere follows the first
// bone, with room for limbs that swing out of the bind pose's bounds.
BoundingSphere skinned_sphere(const Bounds& bounds, const Matrix& first_bone);
// The smallest angle (radians) a part may look to still be drawn: `asked` when positive, or the
// usual one of the pass.
float smallest_angle_drawn(float asked, bool depth_pass);
// False for objects that are never drawn: destroyed, hidden, without a (known) mesh, or prefabs
// outside the editor.
bool has_something_to_draw(const GameObject& object, bool show_prefabs, std::size_t mesh_count);

// One view of the scene: what it can see and who judges how big things look.
struct View {
  Frustum frustum;
  Vec3 viewer;
  float min_angle = 0.0f;
};
// True when the sphere is in the view, looks big enough and is within [from, to) meters.
bool worth_drawing(const View& view, const BoundingSphere& sphere, float from, float to);

// What a pass judges one object by, worked out once a frame: the sphere around its mesh where it
// is now. A mesh without bounds is judged by distance alone, from where its object is (the
// sphere's center then).
struct Sight {
  BoundingSphere sphere;
  bool bounded = false;
};
Sight sight_of(const Bounds& bounds, const Transform& world);
// The same for a skinned mesh, placed by its skeleton's first bone.
Sight skinned_sight_of(const Bounds& bounds, const Transform& world, const Matrix& first_bone);
// Whether a view draws what has that sight and is drawn within [from, to) meters.
bool sighted(const View& view, const Sight& sight, float from, float to);

}  // namespace redgem::render_math
