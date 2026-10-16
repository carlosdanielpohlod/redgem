#pragma once

// Geometry inside a .glb: where the file keeps a mesh's vertices, and how a shape made in the
// editor (its vertex streams and indices) is added to the file.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "editor/document/glb_file.hpp"
#include "scene/scene_desc.hpp"

namespace redgem::glb_geometry {

// Where in a .glb's binary chunk of `bin_size` bytes an accessor keeps `float_count` floats, when
// it keeps them the way the editor writes positions and normals: three floats a vertex, packed,
// in the file's only buffer, wholly inside its buffer view and the chunk. nullopt for anything
// else, so that nothing is ever written where the file merely says its data is.
std::optional<std::size_t> vec3_stream_offset(const nlohmann::json& gltf, std::size_t accessor_index,
                                              std::size_t float_count, std::size_t bin_size);

// The box an accessor's positions fit in, as glTF wants it ("min" and "max").
void write_bounds(nlohmann::json& accessor, const Bounds& bounds);

// Appends raw bytes to the binary chunk, aligned; returns where they start.
std::size_t append_binary(GlbFile& glb, const std::uint8_t* data, std::size_t bytes);

// A mesh of one part: its vertex streams and indices go into the file as new buffer views and
// accessors (the positions with `bounds`), and the mesh after the file's others. Returns the
// mesh's index.
int append_mesh(GlbFile& glb, const std::string& name, const PrimitiveDesc& primitive, const Bounds& bounds);

}  // namespace redgem::glb_geometry
