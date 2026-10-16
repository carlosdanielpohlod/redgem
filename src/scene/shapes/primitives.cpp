#include "scene/shapes/primitives.hpp"

#include "scene/shapes/primitive_builder.hpp"
#include "scene/shapes/primitive_round_shapes.hpp"
#include "scene/props.hpp"

namespace redgem {
namespace {

constexpr float kCubeHalf = 0.5f;
constexpr float kPlaneHalf = 5.0f;

// A cube's face: its normal, then the two in-plane axes (u, v) so that u x v = normal.
struct CubeFace {
  Vec3 normal;
  Vec3 u;
  Vec3 v;
};
constexpr CubeFace kCubeFaces[] = {
    {{1, 0, 0}, {0, 0, -1}, {0, 1, 0}}, {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}}, {{0, 1, 0}, {1, 0, 0}, {0, 0, -1}},
    {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}}, {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}},  {{0, 0, -1}, {-1, 0, 0}, {0, 1, 0}},
};

// Where a quad's corners are along its two axes, counter-clockwise.
struct QuadCorner {
  float u;
  float v;
};
constexpr QuadCorner kQuadCorners[] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};

void cube_face(PrimitiveBuilder& b, const CubeFace& face) {
  const std::uint32_t first = b.next_index();
  for (const QuadCorner& corner : kQuadCorners) {
    const Vec3 p = (face.normal + face.u * corner.u + face.v * corner.v) * kCubeHalf;
    b.vertex(p, face.normal, (corner.u + 1) * kHalf, (corner.v + 1) * kHalf);
  }
  b.quad_from(first);
}

PrimitiveDesc make_cube() {
  PrimitiveBuilder b;
  for (const CubeFace& face : kCubeFaces) cube_face(b, face);
  return b.out;
}

PrimitiveDesc make_plane() {
  PrimitiveBuilder b;
  const std::uint32_t first = b.vertex({-kPlaneHalf, 0, kPlaneHalf}, Vec3::up(), 0, 0);
  b.vertex({kPlaneHalf, 0, kPlaneHalf}, Vec3::up(), 1, 0);
  b.vertex({kPlaneHalf, 0, -kPlaneHalf}, Vec3::up(), 1, 1);
  b.vertex({-kPlaneHalf, 0, -kPlaneHalf}, Vec3::up(), 0, 1);
  b.quad_from(first);
  return b.out;
}

// What the editor calls each shape.
struct ShapeName {
  PrimitiveShape shape;
  const char* name;
};
constexpr ShapeName kShapeNames[] = {{PrimitiveShape::Cube, "Cube"},
                                     {PrimitiveShape::Sphere, "Sphere"},
                                     {PrimitiveShape::Plane, "Plane"},
                                     {PrimitiveShape::Cylinder, "Cylinder"},
                                     {PrimitiveShape::Capsule, "Capsule"}};

}  // namespace

PrimitiveDesc make_primitive(PrimitiveShape shape) {
  switch (shape) {
    case PrimitiveShape::Cube:
      return make_cube();
    case PrimitiveShape::Sphere:
      return make_sphere();
    case PrimitiveShape::Plane:
      return make_plane();
    case PrimitiveShape::Cylinder:
      return make_cylinder();
    case PrimitiveShape::Capsule:
      return make_capsule();
  }
  return make_cube();
}

const char* primitive_name(PrimitiveShape shape) {
  for (const ShapeName& known : kShapeNames) {
    if (known.shape == shape) return known.name;
  }
  return kShapeNames[0].name;
}

std::optional<PrimitiveShape> primitive_from_name(const std::string& name) {
  for (const PrimitiveShape shape : all_primitives()) {
    if (name == primitive_name(shape)) return shape;
  }
  return std::nullopt;
}

const std::vector<PrimitiveShape>& all_primitives() {
  static const std::vector<PrimitiveShape> shapes = {PrimitiveShape::Cube, PrimitiveShape::Sphere,
                                                     PrimitiveShape::Plane, PrimitiveShape::Cylinder,
                                                     PrimitiveShape::Capsule};
  return shapes;
}

const char* primitive_collider(PrimitiveShape shape) {
  switch (shape) {
    case PrimitiveShape::Sphere:
      return props::kColliderSphere;
    case PrimitiveShape::Capsule:
      return props::kColliderCapsule;
    default:
      return props::kColliderBox;
  }
}

}  // namespace redgem
