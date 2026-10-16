#include "scene/shapes/primitive_round_shapes.hpp"

#include <cmath>

#include "scene/shapes/primitive_builder.hpp"

namespace redgem {
namespace {

constexpr float kRadius = 0.5f;
constexpr float kCylinderHalfHeight = 0.5f;
constexpr float kCapsuleCylinderHalf = 0.5f;  // + two 0.5 m hemispheres = 2 m tall
constexpr int kSegments = 32;
constexpr int kRings = 16;
constexpr int kCapRings = 8;
constexpr float kFullTurn = 2.0f * kPi;
constexpr std::uint32_t kTubeVerticesPerSegment = 2;  // one on the top rim, one on the bottom
// Where the texture runs up a capsule: its hemispheres take a quarter each.
constexpr float kTopV = 1.0f;
constexpr float kQuarterV = 0.75f;
constexpr float kLowerV = 0.25f;
constexpr float kBottomV = 0.0f;

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
void band_ring(PrimitiveBuilder& b, const Band& band, float t) {
  const float theta = band.theta0 + (band.theta1 - band.theta0) * t;
  for (int s = 0; s <= kSegments; ++s) {
    const float phi = segment_angle(s);
    const Vec3 normal{std::sin(theta) * std::cos(phi), std::cos(theta), -std::sin(theta) * std::sin(phi)};
    const float v = band.v0 + (band.v1 - band.v0) * t;
    b.vertex(normal * kRadius + Vec3{0, band.offset, 0}, normal, static_cast<float>(s) / kSegments, v);
  }
}

// Joins `rings` + 1 circles of vertices, the first one starting at `first`, with quads.
void band_quads(PrimitiveBuilder& b, std::uint32_t first, int rings) {
  const std::uint32_t row = kSegments + 1;
  for (int r = 0; r < rings; ++r) {
    for (int s = 0; s < kSegments; ++s) {
      const std::uint32_t a = first + static_cast<std::uint32_t>(r) * row + static_cast<std::uint32_t>(s);
      b.quad(a, a + row, a + row + 1, a + 1);
    }
  }
}

void sphere_band(PrimitiveBuilder& b, const Band& band) {
  const std::uint32_t first = b.next_index();
  for (int r = 0; r <= band.rings; ++r) band_ring(b, band, static_cast<float>(r) / static_cast<float>(band.rings));
  band_quads(b, first, band.rings);
}

void tube(PrimitiveBuilder& b, float half_height, float v0, float v1) {
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
void disc_rim(PrimitiveBuilder& b, float y, Vec3 normal) {
  for (int s = 0; s <= kSegments; ++s) {
    const float x = std::cos(segment_angle(s));
    const float z = -std::sin(segment_angle(s));
    b.vertex({x * kRadius, y, z * kRadius}, normal, (x + 1) * kHalf, (z + 1) * kHalf);
  }
}

void disc(PrimitiveBuilder& b, float y, Vec3 normal) {
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

}  // namespace

PrimitiveDesc make_sphere() {
  PrimitiveBuilder b;
  sphere_band(b, {0.0f, kPi, kRings, 0.0f, kTopV, kBottomV});
  return b.out;
}

PrimitiveDesc make_cylinder() {
  PrimitiveBuilder b;
  tube(b, kCylinderHalfHeight, kTopV, kBottomV);
  disc(b, kCylinderHalfHeight, Vec3::up());
  disc(b, -kCylinderHalfHeight, -Vec3::up());
  return b.out;
}

PrimitiveDesc make_capsule() {
  PrimitiveBuilder b;
  sphere_band(b, {0.0f, kPi * kHalf, kCapRings, kCapsuleCylinderHalf, kTopV, kQuarterV});
  tube(b, kCapsuleCylinderHalf, kQuarterV, kLowerV);
  sphere_band(b, {kPi * kHalf, kPi, kCapRings, -kCapsuleCylinderHalf, kLowerV, kBottomV});
  return b.out;
}

}  // namespace redgem
