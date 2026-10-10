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
constexpr int kCubeFaces = 6;
constexpr int kQuadCorners = 4;
constexpr float kFullTurn = 2.0f * kPi;

struct Builder {
  PrimitiveDesc out;

  std::uint32_t vertex(Vec3 position, Vec3 normal, float u, float v) {
    out.positions.insert(out.positions.end(), {position.x, position.y, position.z});
    out.normals.insert(out.normals.end(), {normal.x, normal.y, normal.z});
    out.texcoords.insert(out.texcoords.end(), {u, v});
    return static_cast<std::uint32_t>(out.positions.size() / config::kFloatsPerPosition - 1);
  }
  // Counter-clockwise when seen from the side the normal points to.
  void triangle(std::uint32_t a, std::uint32_t b, std::uint32_t c) { out.indices.insert(out.indices.end(), {a, b, c}); }
  void quad(std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
    triangle(a, b, c);
    triangle(a, c, d);
  }
};

PrimitiveDesc cube() {
  Builder b;
  // Each face: normal, then the two in-plane axes (u, v) so that u x v = normal.
  const Vec3 faces[kCubeFaces][3] = {
      {{1, 0, 0}, {0, 0, -1}, {0, 1, 0}}, {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}},
      {{0, 1, 0}, {1, 0, 0}, {0, 0, -1}}, {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}},
      {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}},  {{0, 0, -1}, {-1, 0, 0}, {0, 1, 0}},
  };
  const float corners[kQuadCorners][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
  for (const auto& face : faces) {
    std::uint32_t ids[kQuadCorners];
    for (int c = 0; c < kQuadCorners; ++c) {
      const Vec3 p = (face[0] + face[1] * corners[c][0] + face[2] * corners[c][1]) * kCubeHalf;
      ids[c] = b.vertex(p, face[0], (corners[c][0] + 1) * kHalf, (corners[c][1] + 1) * kHalf);
    }
    b.quad(ids[0], ids[1], ids[2], ids[3]);
  }
  return b.out;
}

PrimitiveDesc plane() {
  Builder b;
  const std::uint32_t a = b.vertex({-kPlaneHalf, 0, kPlaneHalf}, Vec3::up(), 0, 0);
  const std::uint32_t c1 = b.vertex({kPlaneHalf, 0, kPlaneHalf}, Vec3::up(), 1, 0);
  const std::uint32_t c2 = b.vertex({kPlaneHalf, 0, -kPlaneHalf}, Vec3::up(), 1, 1);
  const std::uint32_t c3 = b.vertex({-kPlaneHalf, 0, -kPlaneHalf}, Vec3::up(), 0, 1);
  b.quad(a, c1, c2, c3);
  return b.out;
}

// Latitude/longitude surface between polar angles [theta0, theta1], shifted up/down by `offset`.
void sphere_band(Builder& b, float theta0, float theta1, int rings, float offset, float v0, float v1) {
  const auto first = static_cast<std::uint32_t>(b.out.positions.size() / config::kFloatsPerPosition);
  for (int r = 0; r <= rings; ++r) {
    const float t = static_cast<float>(r) / static_cast<float>(rings);
    const float theta = theta0 + (theta1 - theta0) * t;
    for (int s = 0; s <= kSegments; ++s) {
      const float phi = kFullTurn * static_cast<float>(s) / static_cast<float>(kSegments);
      const Vec3 normal{std::sin(theta) * std::cos(phi), std::cos(theta), -std::sin(theta) * std::sin(phi)};
      b.vertex(normal * kRadius + Vec3{0, offset, 0}, normal, static_cast<float>(s) / kSegments, v0 + (v1 - v0) * t);
    }
  }
  const std::uint32_t row = kSegments + 1;
  for (int r = 0; r < rings; ++r) {
    for (int s = 0; s < kSegments; ++s) {
      const std::uint32_t a = first + static_cast<std::uint32_t>(r) * row + static_cast<std::uint32_t>(s);
      b.quad(a, a + row, a + row + 1, a + 1);
    }
  }
}

PrimitiveDesc sphere() {
  Builder b;
  sphere_band(b, 0.0f, kPi, kRings, 0.0f, 1.0f, 0.0f);
  return b.out;
}

void tube(Builder& b, float half_height, float v0, float v1) {
  const auto first = static_cast<std::uint32_t>(b.out.positions.size() / config::kFloatsPerPosition);
  for (int s = 0; s <= kSegments; ++s) {
    const float phi = kFullTurn * static_cast<float>(s) / static_cast<float>(kSegments);
    const Vec3 normal{std::cos(phi), 0, -std::sin(phi)};
    const float u = static_cast<float>(s) / kSegments;
    b.vertex(normal * kRadius + Vec3{0, half_height, 0}, normal, u, v0);
    b.vertex(normal * kRadius - Vec3{0, half_height, 0}, normal, u, v1);
  }
  for (int s = 0; s < kSegments; ++s) {
    const std::uint32_t a = first + static_cast<std::uint32_t>(s) * 2;
    b.quad(a, a + 1, a + 3, a + 2);
  }
}

void disc(Builder& b, float y, Vec3 normal) {
  const std::uint32_t center = b.vertex({0, y, 0}, normal, kHalf, kHalf);
  const auto first = static_cast<std::uint32_t>(b.out.positions.size() / config::kFloatsPerPosition);
  for (int s = 0; s <= kSegments; ++s) {
    const float phi = kFullTurn * static_cast<float>(s) / static_cast<float>(kSegments);
    const float x = std::cos(phi);
    const float z = -std::sin(phi);
    b.vertex({x * kRadius, y, z * kRadius}, normal, (x + 1) * kHalf, (z + 1) * kHalf);
  }
  for (int s = 0; s < kSegments; ++s) {
    const std::uint32_t a = first + static_cast<std::uint32_t>(s);
    if (normal.y > 0) {
      b.triangle(center, a, a + 1);
    } else {
      b.triangle(center, a + 1, a);
    }
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
  sphere_band(b, 0.0f, kPi * kHalf, kCapRings, kCapsuleCylinderHalf, kTopV, kQuarterV);
  tube(b, kCapsuleCylinderHalf, kQuarterV, kLowerV);
  sphere_band(b, kPi * kHalf, kPi, kCapRings, -kCapsuleCylinderHalf, kLowerV, kBottomV);
  return b.out;
}

}  // namespace

PrimitiveDesc make_primitive(PrimitiveShape shape) {
  switch (shape) {
    case PrimitiveShape::Cube: return cube();
    case PrimitiveShape::Sphere: return sphere();
    case PrimitiveShape::Plane: return plane();
    case PrimitiveShape::Cylinder: return cylinder();
    case PrimitiveShape::Capsule: return capsule();
  }
  return cube();
}

const char* primitive_name(PrimitiveShape shape) {
  switch (shape) {
    case PrimitiveShape::Cube: return "Cube";
    case PrimitiveShape::Sphere: return "Sphere";
    case PrimitiveShape::Plane: return "Plane";
    case PrimitiveShape::Cylinder: return "Cylinder";
    case PrimitiveShape::Capsule: return "Capsule";
  }
  return "Cube";
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
    case PrimitiveShape::Sphere: return "sphere";
    case PrimitiveShape::Capsule: return "capsule";
    default: return "box";
  }
}

}  // namespace redgem
