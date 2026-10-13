#pragma once

// What a glTF accessor holds, as a scene keeps it. Each of these checks the accessor first (see
// gltf_checks::readable): one that cannot be read gives nothing, and whoever asked leaves out
// what needed it.

#include <cgltf.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace redgem::gltf_accessors {

inline constexpr cgltf_uint kMaxJointByte = 255;  // the highest joint index a vertex can name

// Every component of every element, as floats.
std::vector<float> unpack_floats(const cgltf_accessor* accessor);

// The joints each vertex follows, four to a vertex, one byte each: an index past what a byte
// holds becomes the highest one it does.
std::vector<std::uint8_t> joint_bytes(const cgltf_accessor* accessor);

// Vertex colors as four bytes a vertex. Colors without an alpha are opaque; channels outside
// 0..1 are brought into it.
std::vector<std::uint8_t> rgba_bytes(const cgltf_accessor* accessor);

// The corners of a mesh's triangles: as `listed` gives them, or every one of `vertex_count`
// vertices in turn when the mesh lists none (`listed` is null).
std::vector<std::uint32_t> triangle_corners(const cgltf_accessor* listed, std::size_t vertex_count);

}  // namespace redgem::gltf_accessors
