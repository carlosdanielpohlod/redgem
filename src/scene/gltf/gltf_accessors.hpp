#pragma once

// What a glTF accessor holds, as a scene keeps it. Each of these checks the accessor first (see
// gltf_checks::readable): one that cannot be read gives nothing, and whoever asked leaves out
// what needed it.

#include <cgltf.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "scene/scene_config.hpp"

namespace redgem::gltf_accessors {

// The highest joint index a vertex can name: the last joint a skeleton may have.
inline constexpr cgltf_uint kMaxJointIndex = config::kMaxSkinJoints - 1;

// Every component of every element, as floats.
std::vector<float> unpack_floats(const cgltf_accessor* accessor);

// The joints each vertex follows, four to a vertex: an index past the last joint a skeleton may
// have becomes that one.
std::vector<std::uint16_t> joint_indices(const cgltf_accessor* accessor);

// Vertex colors as four bytes a vertex. Colors without an alpha are opaque; channels outside
// 0..1 are brought into it.
std::vector<std::uint8_t> rgba_bytes(const cgltf_accessor* accessor);

// The corners of a mesh's triangles: as `listed` gives them, or every one of `vertex_count`
// vertices in turn when the mesh lists none (`listed` is null).
std::vector<std::uint32_t> triangle_corners(const cgltf_accessor* listed, std::size_t vertex_count);

}  // namespace redgem::gltf_accessors
