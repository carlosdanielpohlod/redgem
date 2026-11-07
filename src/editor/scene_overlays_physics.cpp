#include "editor/scene_overlays.hpp"

#include "render/render_math.hpp"

namespace redgem {
namespace {
constexpr ::Color kStatic{160, 180, 200, 255};
constexpr ::Color kDynamic{80, 240, 120, 255};
constexpr ::Color kKinematic{100, 160, 255, 255};
constexpr ::Color kCharacter{80, 230, 255, 255};
constexpr ::Color kTrigger{255, 180, 60, 255};
::Color collider_color(const PhysicsDebugTriangle& triangle) {
  if (triangle.trigger) return kTrigger;
  switch (triangle.type) {
    case BodyType::Dynamic:
    case BodyType::Vehicle:
      return kDynamic;
    case BodyType::Kinematic:
      return kKinematic;
    case BodyType::Character:
      return kCharacter;
    default:
      return kStatic;
  }
}
void draw_triangle(const PhysicsDebugTriangle& triangle) {
  for (std::size_t corner = 0; corner < kDebugTriangleCorners; ++corner) {
    const auto next = (corner + 1) % kDebugTriangleCorners;
    DrawLine3D(render_math::to_raylib(triangle.corners[corner]), render_math::to_raylib(triangle.corners[next]),
               collider_color(triangle));
  }
}
}  // namespace
void draw_physics_colliders(const PhysicsDebugGeometry& geometry) {
  for (const auto& triangle : geometry.triangles) draw_triangle(triangle);
}
}  // namespace redgem
