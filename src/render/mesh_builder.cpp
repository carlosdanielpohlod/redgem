#include "render/mesh_builder.hpp"

#include <cstdint>
#include <cstring>

#include "render/mesh_pieces.hpp"
#include "render/render_config.hpp"
#include "scene/scene_config.hpp"

namespace redgem {
namespace {

// Room for `count` values, all zero (from raylib's allocator: raylib frees the mesh).
template <typename T>
T* zeroed_array(std::size_t count) {
  return static_cast<T*>(MemAlloc(static_cast<unsigned int>(count * sizeof(T))));
}

template <typename T>
T* copy_array(const T* data, std::size_t count) {
  T* out = zeroed_array<T>(count);
  std::memcpy(out, data, count * sizeof(T));
  return out;
}

// Vertices without a color of their own are white: the material's color shows unchanged.
unsigned char* white_colors(std::size_t channel_count) {
  auto* colors = zeroed_array<unsigned char>(channel_count);
  std::memset(colors, config::kMaxColorByte, channel_count);
  return colors;
}

// raylib meshes use 16-bit indices (build_mesh has checked that they fit).
unsigned short* short_indices(const std::vector<std::uint32_t>& indices) {
  auto* out = zeroed_array<unsigned short>(indices.size());
  for (std::size_t i = 0; i < indices.size(); ++i) out[i] = static_cast<unsigned short>(indices[i]);
  return out;
}

// What a vertex has besides its place and normal: texture coordinates (zero when the primitive
// has none), a color (white likewise) and, for skinned meshes, the weights of its joints (the
// joints themselves go up on their own: see skin_joints).
void copy_vertex_data(Mesh& mesh, const PrimitiveDesc& primitive) {
  const std::size_t texcoord_count = vertex_count_of(primitive) * config::kFloatsPerTexcoord;
  const std::size_t color_count = vertex_count_of(primitive) * config::kChannelsPerColor;
  const VertexData has = vertex_data_of(primitive);
  mesh.texcoords =
      has.texcoords ? copy_array(primitive.texcoords.data(), texcoord_count) : zeroed_array<float>(texcoord_count);
  mesh.colors = has.colors ? copy_array(primitive.colors.data(), color_count) : white_colors(color_count);
  if (!has.skin) return;
  mesh.boneWeights = copy_array(primitive.weights.data(), primitive.weights.size());
}

}  // namespace

std::string primitive_problem(const PrimitiveDesc& primitive) {
  const std::size_t vertex_count = vertex_count_of(primitive);
  if (vertex_count > config::kMaxVerticesPerMesh) {
    return "primitive has " + std::to_string(vertex_count) + " vertices, the limit is " +
           std::to_string(config::kMaxVerticesPerMesh);
  }
  return primitive_shape_problem(primitive);
}

std::optional<Mesh> build_mesh(const PrimitiveDesc& primitive, std::string& error) {
  const std::string problem = primitive_problem(primitive);
  if (!problem.empty()) {
    error = problem;
    return std::nullopt;
  }
  Mesh mesh{};
  mesh.vertexCount = static_cast<int>(vertex_count_of(primitive));
  mesh.triangleCount = static_cast<int>(primitive.indices.size() / config::kVerticesPerTriangle);
  mesh.vertices = copy_array(primitive.positions.data(), primitive.positions.size());
  mesh.normals = copy_array(primitive.normals.data(), primitive.normals.size());
  copy_vertex_data(mesh, primitive);
  mesh.indices = short_indices(primitive.indices);
  return mesh;
}

std::vector<std::uint16_t> skin_joints(const PrimitiveDesc& primitive) {
  return vertex_data_of(primitive).skin ? primitive.joints : std::vector<std::uint16_t>{};
}

void free_cpu_mesh(Mesh& mesh) {
  MemFree(mesh.vertices);
  MemFree(mesh.normals);
  MemFree(mesh.texcoords);
  MemFree(mesh.colors);
  MemFree(mesh.indices);
  if (mesh.boneWeights != nullptr) MemFree(mesh.boneWeights);
  mesh = Mesh{};
}

}  // namespace redgem
