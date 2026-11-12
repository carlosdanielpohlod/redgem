#include "scene/scene_desc.hpp"

#include <algorithm>
#include <cmath>

#include "scene/scene_config.hpp"

namespace redgem {
namespace {

bool whole_triangles_of(const std::vector<std::uint32_t>& indices, std::size_t vertex_count) {
  if (indices.size() % config::kVerticesPerTriangle != 0) return false;
  return std::all_of(indices.begin(), indices.end(),
                     [vertex_count](std::uint32_t index) { return index < vertex_count; });
}

}  // namespace

std::string primitive_shape_problem(const PrimitiveDesc& primitive) {
  const std::size_t vertex_count = primitive.positions.size() / config::kFloatsPerPosition;
  if (vertex_count == 0) return "primitive has no vertices";
  const bool whole_vertices = primitive.positions.size() % config::kFloatsPerPosition == 0 &&
                              primitive.normals.size() == primitive.positions.size();
  if (!whole_vertices) return "primitive has vertices without a whole position and normal";
  if (!whole_triangles_of(primitive.indices, vertex_count)) return "primitive has invalid triangle indices";
  return {};
}

void keep_joints_within(PrimitiveDesc& primitive, std::size_t joints) {
  for (std::uint16_t& joint : primitive.joints) {
    if (joint >= joints) joint = 0;
  }
}

float maximum_spot_inner_cone(float outer) {
  // The degree pair was checked before conversion. Allow the last rounding step of a float.
  return std::nextafter(outer - kMinSpotConeGap, outer);
}

float minimum_spot_outer_cone(float inner) {
  const float sum = std::min(inner + kMinSpotConeGap, kMaxSpotOuterCone);
  return maximum_spot_inner_cone(sum) < inner ? std::nextafter(sum, kMaxSpotOuterCone) : sum;
}

void sanitize_spot_cones(LightDesc& light) {
  if (!is_spot_light(light)) return;
  const bool outer_valid = std::isfinite(light.outer_cone) && light.outer_cone > kSpotConeAxis;
  light.outer_cone =
      outer_valid ? std::clamp(light.outer_cone, kMinSpotConeGap, kMaxSpotOuterCone) : kDefaultSpotOuterCone;
  const float max_inner = maximum_spot_inner_cone(light.outer_cone);
  light.inner_cone =
      std::isfinite(light.inner_cone) ? std::clamp(light.inner_cone, kSpotConeAxis, max_inner) : kSpotConeAxis;
}

namespace {
bool is_valid_cone_pair(double inner, double outer) {
  const auto minimum = std::nextafter(inner + kMinSpotConeGapDegrees, 0.0);
  return inner >= 0.0 && outer >= minimum && outer <= kMaxSpotConeDegrees;
}

constexpr double kConeStepsPerDegree = 10'000;
constexpr double kConeGapSteps = kMinSpotConeGapDegrees * kConeStepsPerDegree;

double cone_degree_steps(float radians) { return std::round(radians / kSpotRadiansPerDegree * kConeStepsPerDegree); }
}  // namespace

bool set_spot_cone_degrees(LightDesc& light, double inner, double outer) {
  if (!is_spot_light(light) || !is_valid_cone_pair(inner, outer)) return false;
  light.inner_cone = static_cast<float>(inner * kSpotRadiansPerDegree);
  light.outer_cone = static_cast<float>(outer * kSpotRadiansPerDegree);
  sanitize_spot_cones(light);
  return true;
}

SpotConeDegrees spot_cone_degrees(const LightDesc& light) {
  const double outer = cone_degree_steps(light.outer_cone);
  const double inner = std::min(cone_degree_steps(light.inner_cone), outer - kConeGapSteps);
  return {inner / kConeStepsPerDegree, outer / kConeStepsPerDegree};
}

void Bounds::include(Vec3 point) {
  if (!valid) {
    min = max = point;
    valid = true;
    return;
  }
  min = {std::min(min.x, point.x), std::min(min.y, point.y), std::min(min.z, point.z)};
  max = {std::max(max.x, point.x), std::max(max.y, point.y), std::max(max.z, point.z)};
}

int SceneDesc::find_node(const std::string& name) const {
  for (std::size_t i = 0; i < nodes.size(); ++i) {
    if (nodes[i].name == name) return static_cast<int>(i);
  }
  return kNoIndex;
}

}  // namespace redgem
