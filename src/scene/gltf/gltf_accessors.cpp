#include "scene/gltf/gltf_accessors.hpp"

#include <algorithm>

#include "scene/gltf/gltf_checks.hpp"
#include "scene/scene_config.hpp"

namespace redgem::gltf_accessors {
namespace {

constexpr int kJointsPerVertex = config::kJointsPerVertex;
constexpr cgltf_size kRgbaComponents = config::kChannelsPerColor;
constexpr float kOpaqueAlpha = 1.0f;

std::uint8_t color_byte(float channel) {
  return static_cast<std::uint8_t>(std::clamp(channel, 0.0f, 1.0f) * config::kMaxColorByte);
}

// One vertex's color out of `components` floats: the channels it has, and opaque for the rest.
void write_rgba(const float* color, cgltf_size components, std::uint8_t* out) {
  for (cgltf_size c = 0; c < kRgbaComponents; ++c) out[c] = color_byte(c < components ? color[c] : kOpaqueAlpha);
}

std::vector<std::uint32_t> every_vertex_in_turn(std::size_t vertex_count) {
  std::vector<std::uint32_t> corners(vertex_count);
  for (std::size_t k = 0; k < vertex_count; ++k) corners[k] = static_cast<std::uint32_t>(k);
  return corners;
}

}  // namespace

std::vector<float> unpack_floats(const cgltf_accessor* accessor) {
  if (!gltf_checks::readable(accessor)) return {};
  const cgltf_size components = cgltf_num_components(accessor->type);
  std::vector<float> out(accessor->count * components);
  cgltf_accessor_unpack_floats(accessor, out.data(), out.size());
  return out;
}

std::vector<std::uint16_t> joint_indices(const cgltf_accessor* accessor) {
  if (!gltf_checks::readable(accessor)) return {};
  std::vector<std::uint16_t> out(accessor->count * kJointsPerVertex);
  for (cgltf_size v = 0; v < accessor->count; ++v) {
    cgltf_uint joints[kJointsPerVertex] = {};
    cgltf_accessor_read_uint(accessor, v, joints, kJointsPerVertex);
    for (int j = 0; j < kJointsPerVertex; ++j) {
      out[v * kJointsPerVertex + j] = static_cast<std::uint16_t>(std::min<cgltf_uint>(joints[j], kMaxJointIndex));
    }
  }
  return out;
}

std::vector<std::uint8_t> rgba_bytes(const cgltf_accessor* accessor) {
  const std::vector<float> floats = unpack_floats(accessor);
  if (floats.empty()) return {};
  const cgltf_size components = cgltf_num_components(accessor->type);
  std::vector<std::uint8_t> out(accessor->count * kRgbaComponents);
  for (cgltf_size v = 0; v < accessor->count; ++v) {
    write_rgba(&floats[v * components], components, &out[v * kRgbaComponents]);
  }
  return out;
}

std::vector<std::uint32_t> triangle_corners(const cgltf_accessor* listed, std::size_t vertex_count) {
  if (listed == nullptr) return every_vertex_in_turn(vertex_count);
  if (!gltf_checks::readable(listed)) return {};
  std::vector<std::uint32_t> corners(listed->count);
  for (std::size_t k = 0; k < corners.size(); ++k) {
    corners[k] = static_cast<std::uint32_t>(cgltf_accessor_read_index(listed, k));
  }
  return corners;
}

}  // namespace redgem::gltf_accessors
