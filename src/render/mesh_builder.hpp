#pragma once

#include <raylib.h>

#include <cstddef>
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

// Splits a primitive into pieces of at most `max_vertices` vertices each (raylib meshes use 16-bit
// indices). Triangles are kept whole; each piece holds only the vertices its triangles use.
std::vector<PrimitiveDesc> split_primitive(const PrimitiveDesc& primitive, std::size_t max_vertices);

// The pieces a mesh is drawn as, in order: each of its primitives, split where it has more than
// `max_vertices`. The same mesh with its vertices moved gives the same pieces, moved: what is on
// the graphics card and what a reshaped mesh is cut into line up, piece for piece.
std::vector<PrimitiveDesc> mesh_pieces(const MeshDesc& mesh, std::size_t max_vertices);

// Frees the CPU arrays of a mesh produced by build_mesh that was never uploaded.
void free_cpu_mesh(Mesh& mesh);

}  // namespace redgem
