#include "scene/ground/ground_survey.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace redgem {
namespace {

constexpr float kSmallestSpan = 1.0f;  // meters: what a flat side of the mesh is spread over
constexpr float kFlatArea = 1e-12f;    // a triangle seen from above with less area has none
// How far outside a triangle a point may be and still count as on it: a point on an edge two
// triangles share belongs to both, whatever the rounding.
constexpr float kEdgeTolerance = -1e-5f;
constexpr std::size_t kXyz = 3;
constexpr std::size_t kCorners = 3;
// A triangle's corners, in order.
constexpr std::size_t kA = 0;
constexpr std::size_t kB = 1;
constexpr std::size_t kC = 2;

float span(float low, float high) { return std::max(high - low, kSmallestSpan); }

// One triangle of a part: its corners and their normals.
struct Triangle {
  std::array<Vec3, kCorners> corners;
  std::array<Vec3, kCorners> normals;
};

Vec3 at(const std::vector<float>& values, std::uint32_t vertex) {
  return vec3_from(&values[static_cast<std::size_t>(vertex) * kXyz]);
}

// How much of each corner a point (x, z) is made of, seen from above; nothing outside.
struct Shares {
  bool inside = false;
  std::array<float, kCorners> of{};
};

Shares shares_at(const Triangle& triangle, float x, float z) {
  const Vec3& a = triangle.corners[kA];
  const Vec3& b = triangle.corners[kB];
  const Vec3& c = triangle.corners[kC];
  const float area = (b.x - a.x) * (c.z - a.z) - (c.x - a.x) * (b.z - a.z);
  Shares shares;
  if (std::abs(area) < kFlatArea) return shares;
  shares.of[kB] = ((x - a.x) * (c.z - a.z) - (c.x - a.x) * (z - a.z)) / area;
  shares.of[kC] = ((b.x - a.x) * (z - a.z) - (x - a.x) * (b.z - a.z)) / area;
  shares.of[kA] = 1.0f - shares.of[kB] - shares.of[kC];
  shares.inside = std::all_of(shares.of.begin(), shares.of.end(), [](float share) { return share >= kEdgeTolerance; });
  return shares;
}

template <typename Value>
Value mixed(const std::array<Value, kCorners>& values, const Shares& shares) {
  return values[kA] * shares.of[kA] + values[kB] * shares.of[kB] + values[kC] * shares.of[kC];
}

float slope_of(Vec3 normal) {
  const float size = length(normal);
  if (size <= 0.0f) return 0.0f;
  return radians_to_degrees(std::acos(std::clamp(std::abs(normal.y) / size, 0.0f, 1.0f)));
}

// The texels whose middles a triangle may cover: the first and last column and row.
struct TexelBox {
  int first_column = 0;
  int last_column = -1;
  int first_row = 0;
  int last_row = -1;
};

int first_texel(float low, float origin, float size, int texels) {
  return std::max(0, static_cast<int>(std::ceil((low - origin) / size * static_cast<float>(texels) - kHalf)));
}

int last_texel(float high, float origin, float size, int texels) {
  return std::min(texels - 1,
                  static_cast<int>(std::floor((high - origin) / size * static_cast<float>(texels) - kHalf)));
}

TexelBox box_of(const Triangle& triangle, const SplatArea& area, int size) {
  const auto [low_x, high_x] = std::minmax({triangle.corners[kA].x, triangle.corners[kB].x, triangle.corners[kC].x});
  const auto [low_z, high_z] = std::minmax({triangle.corners[kA].z, triangle.corners[kB].z, triangle.corners[kC].z});
  return {first_texel(low_x, area.min_x, area.size_x, size), last_texel(high_x, area.min_x, area.size_x, size),
          first_texel(low_z, area.min_z, area.size_z, size), last_texel(high_z, area.min_z, area.size_z, size)};
}

// What a triangle says of the ground under the texel, kept when it is higher than what was there.
void keep_highest(GroundPoint& point, const Triangle& triangle, const Shares& shares) {
  const float height = mixed(
      std::array<float, kCorners>{triangle.corners[kA].y, triangle.corners[kB].y, triangle.corners[kC].y}, shares);
  if (point.covered && point.height >= height) return;
  point = GroundPoint{true, height, slope_of(mixed(triangle.normals, shares))};
}

std::size_t texel_index(int size, int column, int row) {
  return static_cast<std::size_t>(row) * static_cast<std::size_t>(size) + static_cast<std::size_t>(column);
}

struct Survey {
  SplatArea area;
  int size = 0;
  std::vector<GroundPoint> points;
};

void survey_triangle(Survey& survey, const Triangle& triangle) {
  const TexelBox box = box_of(triangle, survey.area, survey.size);
  for (int row = box.first_row; row <= box.last_row; ++row) {
    for (int column = box.first_column; column <= box.last_column; ++column) {
      const GroundPlace place = texel_place(survey.area, survey.size, column, row);
      const Shares shares = shares_at(triangle, place.x, place.z);
      if (shares.inside) keep_highest(survey.points[texel_index(survey.size, column, row)], triangle, shares);
    }
  }
}

// A triangle's normals: the part's own, or the face's when it has none.
std::array<Vec3, kCorners> normals_of(const PrimitiveDesc& part, const std::array<std::uint32_t, kCorners>& vertices,
                                      const std::array<Vec3, kCorners>& corners) {
  if (part.normals.size() != part.positions.size()) {
    const Vec3 face = cross(corners[kB] - corners[kA], corners[kC] - corners[kA]);
    return {face, face, face};
  }
  return {at(part.normals, vertices[kA]), at(part.normals, vertices[kB]), at(part.normals, vertices[kC])};
}

std::uint32_t vertex_of(const PrimitiveDesc& part, std::size_t corner) {
  return part.indices.empty() ? static_cast<std::uint32_t>(corner) : part.indices[corner];
}

bool whole_triangle(const PrimitiveDesc& part, const std::array<std::uint32_t, kCorners>& vertices) {
  const std::size_t count = part.positions.size() / kXyz;
  return std::all_of(vertices.begin(), vertices.end(), [count](std::uint32_t vertex) { return vertex < count; });
}

void survey_part(Survey& survey, const PrimitiveDesc& part) {
  const std::size_t corners = part.indices.empty() ? part.positions.size() / kXyz : part.indices.size();
  for (std::size_t first = 0; first + kCorners <= corners; first += kCorners) {
    const std::array<std::uint32_t, kCorners> vertices{vertex_of(part, first + kA), vertex_of(part, first + kB),
                                                       vertex_of(part, first + kC)};
    if (!whole_triangle(part, vertices)) continue;
    const std::array<Vec3, kCorners> positions{at(part.positions, vertices[kA]), at(part.positions, vertices[kB]),
                                               at(part.positions, vertices[kC])};
    survey_triangle(survey, Triangle{positions, normals_of(part, vertices, positions)});
  }
}

}  // namespace

SplatArea splat_area(const Bounds& bounds) {
  return {bounds.min.x, bounds.min.z, span(bounds.min.x, bounds.max.x), span(bounds.min.z, bounds.max.z)};
}

SplatSpot splat_spot(const SplatArea& area, float x, float z) {
  return {(x - area.min_x) / area.size_x, (z - area.min_z) / area.size_z};
}

GroundPlace texel_place(const SplatArea& area, int size, int column, int row) {
  const auto texels = static_cast<float>(size);
  return {area.min_x + (static_cast<float>(column) + kHalf) / texels * area.size_x,
          area.min_z + (static_cast<float>(row) + kHalf) / texels * area.size_z};
}

std::vector<GroundPoint> survey_ground(const MeshDesc& mesh, int size) {
  Survey survey{splat_area(mesh.bounds), std::max(size, 0), {}};
  survey.points.resize(static_cast<std::size_t>(survey.size) * static_cast<std::size_t>(survey.size));
  for (const PrimitiveDesc& part : mesh.primitives) survey_part(survey, part);
  return survey.points;
}

}  // namespace redgem
