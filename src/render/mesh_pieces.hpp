#pragma once

// A mesh as the pieces it is drawn in. A piece holds at most as many vertices as 16-bit indices
// can name, so a larger primitive is cut up. No graphics card needed, so it can be tested.

#include <cstddef>
#include <vector>

#include "scene/scene_desc.hpp"

namespace redgem {

// How many vertices a primitive's positions make.
std::size_t vertex_count_of(const PrimitiveDesc& primitive);

// The optional data a primitive carries for every one of its vertices (data of another length
// is as good as none).
struct VertexData {
  bool texcoords = false;
  bool colors = false;
  bool skin = false;
};

VertexData vertex_data_of(const PrimitiveDesc& primitive);

// Splits a primitive into pieces of at most `max_vertices` vertices each (raylib meshes use 16-bit
// indices). Triangles are kept whole; each piece holds only the vertices its triangles use.
std::vector<PrimitiveDesc> split_primitive(const PrimitiveDesc& primitive, std::size_t max_vertices);

// The pieces a mesh is drawn as, in order: each of its primitives, split where it has more than
// `max_vertices`. The same mesh with its vertices moved gives the same pieces, moved: what is on
// the graphics card and what a reshaped mesh is cut into line up, piece for piece.
std::vector<PrimitiveDesc> mesh_pieces(const MeshDesc& mesh, std::size_t max_vertices);

}  // namespace redgem
