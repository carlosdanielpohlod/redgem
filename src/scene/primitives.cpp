#include "scene/primitives.hpp"

#include <cmath>

#include "core/config.hpp"

namespace redgem {
namespace {

constexpr float kCubeHalf = 0.5f;
constexpr float kPlaneHalf = 5.0f;
constexpr float kRadius = 0.5f;
constexpr float kCylinderHalfHeight = 0.5f;
constexpr float kCapsuleCylinderHalf = 0.5f;  // + two 0.5 m hemispheres = 2 m tall
constexpr int kSegments = 32;
constexpr int kRings = 16;
constexpr int kCapRings = 8;
constexpr float kFullTurn = 2.0f * kPi;
constexpr std::uint32_t kTubeVerticesPerSegment = 2;  // one on the top rim, one on the bottom

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

struct Builder {
  PrimitiveDesc out;

  // The index the next vertex will get.
  std::uint32_t next_index() const {
    return static_cast<std::uint32_t>(out.positions.size() / config::kFloatsPerPosition);
  }
  std::uint32_t vertex(Vec3 position, Vec3 normal, float u, float v) {
    out.positions.insert(out.positions.end(), {position.x, position.y, position.z});
    out.normals.insert(out.normals.end(), {normal.x, normal.y, normal.z});
    out.texcoords.insert(out.texcoords.end(), {u, v});
    return next_index() - 1;
  }
  // Counter-clockwise when seen from the side the normal points to.
  void triangle(std::uint32_t a, std::uint32_t b, std::uint32_t c) { out.indices.insert(out.indices.end(), {a, b, c}); }
  void quad(std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
    triangle(a, b, c);
    triangle(a, c, d);
  }
  // A quad whose four corners were added one after the other, `first` first.
  void quad_from(std::uint32_t first) {
    const std::uint32_t second = first + 1;
    const std::uint32_t third = second + 1;
    quad(first, second, third, third + 1);
  }
};

void cube_face(Builder& b, const CubeFace& face) {
  const std::uint32_t first = b.next_index();
  for (const QuadCorner& corner : kQuadCorners) {
    const Vec3 p = (face.normal + face.u * corner.u + face.v * corner.v) * kCubeHalf;
    b.vertex(p, face.normal, (corner.u + 1) * kHalf, (corner.v + 1) * kHalf);
  }
  b.quad_from(first);
}

PrimitiveDesc cube() {
  Builder b;
  for (const CubeFace& face : kCubeFaces) cube_face(b, face);
  return b.out;
}

PrimitiveDesc plane() {
  Builder b;
  const std::uint32_t first = b.vertex({-kPlaneHalf, 0, kPlaneHalf}, Vec3::up(), 0, 0);
  b.vertex({kPlaneHalf, 0, kPlaneHalf}, Vec3::up(), 1, 0);
  b.vertex({kPlaneHalf, 0, -kPlaneHalf}, Vec3::up(), 1, 1);
  b.vertex({-kPlaneHalf, 0, -kPlaneHalf}, Vec3::up(), 0, 1);
  b.quad_from(first);
  return b.out;
}

// How far round the Y axis a segment starts, in radians.
float segment_angle(int segment) { return kFullTurn * static_cast<float>(segment) / static_cast<float>(kSegments); }

// Latitude/longitude surface between polar angles [theta0, theta1], shifted up/down by `offset`,
// its texture running from v0 to v1.
struct Band {
  float theta0;
  float theta1;
  int rings;
  float offset;
  float v0;
  float v1;
};

// One circle of a band's vertices, `t` (0 to 1) of the way down it.
void band_ring(Builder& b, const Band& band, float t) {
  const float theta = band.theta0 + (band.theta1 - band.theta0) * t;
  for (int s = 0; s <= kSegments; ++s) {
    const float phi = segment_angle(s);
    const Vec3 normal{std::sin(theta) * std::cos(phi), std::cos(theta), -std::sin(theta) * std::sin(phi)};
    const float v = band.v0 + (band.v1 - band.v0) * t;
    b.vertex(normal * kRadius + Vec3{0, band.offset, 0}, normal, static_cast<float>(s) / kSegments, v);
  }
}

// Joins `rings` + 1 circles of vertices, the first one starting at `first`, with quads.
void band_quads(Builder& b, std::uint32_t first, int rings) {
  const std::uint32_t row = kSegments + 1;
  for (int r = 0; r < rings; ++r) {
    for (int s = 0; s < kSegments; ++s) {
      const std::uint32_t a = first + static_cast<std::uint32_t>(r) * row + static_cast<std::uint32_t>(s);
      b.quad(a, a + row, a + row + 1, a + 1);
    }
  }
}

void sphere_band(Builder& b, const Band& band) {
  const std::uint32_t first = b.next_index();
  for (int r = 0; r <= band.rings; ++r) band_ring(b, band, static_cast<float>(r) / static_cast<float>(band.rings));
  band_quads(b, first, band.rings);
}

PrimitiveDesc sphere() {
  Builder b;
  sphere_band(b, {0.0f, kPi, kRings, 0.0f, 1.0f, 0.0f});
  return b.out;
}

void tube(Builder& b, float half_height, float v0, float v1) {
  const std::uint32_t first = b.next_index();
  for (int s = 0; s <= kSegments; ++s) {
    const float phi = segment_angle(s);
    const Vec3 normal{std::cos(phi), 0, -std::sin(phi)};
    const float u = static_cast<float>(s) / kSegments;
    b.vertex(normal * kRadius + Vec3{0, half_height, 0}, normal, u, v0);
    b.vertex(normal * kRadius - Vec3{0, half_height, 0}, normal, u, v1);
  }
  for (int s = 0; s < kSegments; ++s) {
    const std::uint32_t top = first + static_cast<std::uint32_t>(s) * kTubeVerticesPerSegment;
    const std::uint32_t next_top = top + kTubeVerticesPerSegment;
    b.quad(top, top + 1, next_top + 1, next_top);
  }
}

// The vertices round the edge of a disc.
void disc_rim(Builder& b, float y, Vec3 normal) {
  for (int s = 0; s <= kSegments; ++s) {
    const float x = std::cos(segment_angle(s));
    const float z = -std::sin(segment_angle(s));
    b.vertex({x * kRadius, y, z * kRadius}, normal, (x + 1) * kHalf, (z + 1) * kHalf);
  }
}

void disc(Builder& b, float y, Vec3 normal) {
  const std::uint32_t center = b.vertex({0, y, 0}, normal, kHalf, kHalf);
  const std::uint32_t first = b.next_index();
  disc_rim(b, y, normal);
  const bool faces_up = normal.y > 0;
  for (int s = 0; s < kSegments; ++s) {
    const std::uint32_t a = first + static_cast<std::uint32_t>(s);
    // Counter-clockwise seen from the side it faces.
    b.triangle(center, faces_up ? a : a + 1, faces_up ? a + 1 : a);
  }
}

PrimitiveDesc cylinder() {
  Builder b;
  tube(b, kCylinderHalfHeight, 1.0f, 0.0f);
  disc(b, kCylinderHalfHeight, Vec3::up());
  disc(b, -kCylinderHalfHeight, -Vec3::up());
  return b.out;
}

PrimitiveDesc capsule() {
  Builder b;
  constexpr float kTopV = 1.0f;
  constexpr float kQuarterV = 0.75f;
  constexpr float kLowerV = 0.25f;
  constexpr float kBottomV = 0.0f;
  sphere_band(b, {0.0f, kPi * kHalf, kCapRings, kCapsuleCylinderHalf, kTopV, kQuarterV});
  tube(b, kCapsuleCylinderHalf, kQuarterV, kLowerV);
  sphere_band(b, {kPi * kHalf, kPi, kCapRings, -kCapsuleCylinderHalf, kLowerV, kBottomV});

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
      return cube();
    case PrimitiveShape::Sphere:
      return sphere();
    case PrimitiveShape::Plane:
      return plane();
    case PrimitiveShape::Cylinder:
      return cylinder();
    case PrimitiveShape::Capsule:
      return capsule();
  }
  return cube();
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
      return config::kColliderSphere;
    case PrimitiveShape::Capsule:
      return config::kColliderCapsule;
    default:
      return config::kColliderBox;
  }
}

}  // namespace redgem
