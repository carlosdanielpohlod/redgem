#pragma once

#include <raylib.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "scene/scene_desc.hpp"

namespace redgem {

// What keeps a primitive from becoming a mesh (no vertices, too many of them, indices that do not
// make whole triangles of its vertices), or an empty text when nothing does.
std::string primitive_problem(const PrimitiveDesc& primitive);

// Converts a glTF primitive into a CPU-side raylib Mesh (not uploaded to the GPU).
// Returns nullopt and fills `error` when the primitive cannot be represented.
std::optional<Mesh> build_mesh(const PrimitiveDesc& primitive, std::string& error);

// The joints of a skinned primitive's vertices, four to a vertex, as they go to the graphics card
// beside the mesh: 16 bits each, where raylib's own mesh holds a byte (256 joints at most). Empty
// for a rigid primitive, or one whose skinning does not have the length its vertices need.
std::vector<std::uint16_t> skin_joints(const PrimitiveDesc& primitive);

// Frees the CPU arrays of a mesh produced by build_mesh that was never uploaded.
void free_cpu_mesh(Mesh& mesh);

}  // namespace redgem
