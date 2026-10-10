#include "render/mesh_builder.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <unordered_map>

#include "core/config.hpp"

namespace redgem {
namespace {

template <typename T>
T* copy_array(const T* data, std::size_t count) {
  auto* out = static_cast<T*>(MemAlloc(static_cast<unsigned int>(count * sizeof(T))));
  std::memcpy(out, data, count * sizeof(T));
  return out;
}

}  // namespace

std::optional<Mesh> build_mesh(const PrimitiveDesc& primitive, std::string& error) {
  const std::size_t vertex_count = primitive.positions.size() / config::kFloatsPerPosition;
  if (vertex_count == 0) {
    error = "primitive has no vertices";
    return std::nullopt;
  }
  if (vertex_count > config::kMaxVerticesPerMesh) {
    error = "primitive has " + std::to_string(vertex_count) + " vertices, the limit is " +
            std::to_string(config::kMaxVerticesPerMesh);
    return std::nullopt;
  }
  const bool bad_index = std::any_of(primitive.indices.begin(), primitive.indices.end(),
                                     [vertex_count](std::uint32_t index) { return index >= vertex_count; });
  if (bad_index || primitive.indices.size() % config::kVerticesPerTriangle != 0) {
    error = "primitive has invalid triangle indices";
    return std::nullopt;
  }

  Mesh mesh{};
  mesh.vertexCount = static_cast<int>(vertex_count);
  mesh.triangleCount = static_cast<int>(primitive.indices.size() / config::kVerticesPerTriangle);
  mesh.vertices = copy_array(primitive.positions.data(), primitive.positions.size());
  mesh.normals = copy_array(primitive.normals.data(), primitive.normals.size());

  const std::size_t texcoord_count = vertex_count * config::kFloatsPerTexcoord;
  if (primitive.texcoords.size() == texcoord_count) {
    mesh.texcoords = copy_array(primitive.texcoords.data(), texcoord_count);
  } else {
    mesh.texcoords = static_cast<float*>(MemAlloc(static_cast<unsigned int>(texcoord_count * sizeof(float))));
  }

  const std::size_t color_count = vertex_count * config::kChannelsPerColor;
  if (primitive.colors.size() == color_count) {
    mesh.colors = copy_array(primitive.colors.data(), color_count);
  } else {
    mesh.colors = static_cast<unsigned char*>(MemAlloc(static_cast<unsigned int>(color_count)));
    std::memset(mesh.colors, config::kMaxColorByte, color_count);
  }

  // Skinned meshes carry their joints and weights to the GPU (raylib's bone attributes).
  const std::size_t influence_count = vertex_count * config::kJointsPerVertex;
  if (primitive.joints.size() == influence_count && primitive.weights.size() == influence_count) {
    mesh.boneIds = copy_array(primitive.joints.data(), influence_count);
    mesh.boneWeights = copy_array(primitive.weights.data(), influence_count);
  }

  mesh.indices = static_cast<unsigned short*>(
      MemAlloc(static_cast<unsigned int>(primitive.indices.size() * sizeof(unsigned short))));
  for (std::size_t i = 0; i < primitive.indices.size(); ++i) {
    mesh.indices[i] = static_cast<unsigned short>(primitive.indices[i]);
  }
  return mesh;
}

std::vector<PrimitiveDesc> split_primitive(const PrimitiveDesc& primitive, std::size_t max_vertices) {
  const std::size_t vertex_count = primitive.positions.size() / config::kFloatsPerPosition;
  if (vertex_count <= max_vertices) return {primitive};
  const bool has_texcoords = primitive.texcoords.size() == vertex_count * config::kFloatsPerTexcoord;
  const bool has_colors = primitive.colors.size() == vertex_count * config::kChannelsPerColor;
  const std::size_t influences = vertex_count * config::kJointsPerVertex;
  const bool has_skin = primitive.joints.size() == influences && primitive.weights.size() == influences;

  std::vector<PrimitiveDesc> pieces;
  PrimitiveDesc piece;
  std::unordered_map<std::uint32_t, std::uint32_t> remap;
  auto start_piece = [&] {
    piece = PrimitiveDesc{};
    piece.material = primitive.material;
    remap.clear();
  };
  auto copy_vertex = [&](std::uint32_t source) {
    auto append = [source](std::vector<float>& out, const std::vector<float>& in, std::size_t width) {
      out.insert(out.end(), in.begin() + static_cast<std::ptrdiff_t>(source * width),
                 in.begin() + static_cast<std::ptrdiff_t>((source + 1) * width));
    };
    append(piece.positions, primitive.positions, config::kFloatsPerPosition);
    append(piece.normals, primitive.normals, config::kFloatsPerNormal);
    if (has_texcoords) append(piece.texcoords, primitive.texcoords, config::kFloatsPerTexcoord);
    if (has_colors) {
      const auto begin = primitive.colors.begin() + static_cast<std::ptrdiff_t>(source * config::kChannelsPerColor);
      piece.colors.insert(piece.colors.end(), begin, begin + config::kChannelsPerColor);
    }
    if (has_skin) {
      const auto joint = primitive.joints.begin() + static_cast<std::ptrdiff_t>(source * config::kJointsPerVertex);
      piece.joints.insert(piece.joints.end(), joint, joint + config::kJointsPerVertex);
      append(piece.weights, primitive.weights, config::kJointsPerVertex);
    }
  };
  start_piece();
  for (std::size_t i = 0; i + config::kVerticesPerTriangle <= primitive.indices.size();
       i += config::kVerticesPerTriangle) {
    std::size_t new_vertices = 0;
    for (std::size_t k = 0; k < config::kVerticesPerTriangle; ++k) {
      new_vertices += remap.count(primitive.indices[i + k]) == 0 ? 1 : 0;
    }
    if (remap.size() + new_vertices > max_vertices) {
      pieces.push_back(std::move(piece));
      start_piece();
    }
    for (std::size_t k = 0; k < config::kVerticesPerTriangle; ++k) {
      const std::uint32_t source = primitive.indices[i + k];
      auto [it, inserted] = remap.emplace(source, static_cast<std::uint32_t>(remap.size()));
      if (inserted) copy_vertex(source);
      piece.indices.push_back(it->second);
    }
  }
  if (!piece.indices.empty()) pieces.push_back(std::move(piece));
  return pieces;
}

void free_cpu_mesh(Mesh& mesh) {
  MemFree(mesh.vertices);
  MemFree(mesh.normals);
  MemFree(mesh.texcoords);
  MemFree(mesh.colors);
  MemFree(mesh.indices);
  if (mesh.boneIds != nullptr) MemFree(mesh.boneIds);
  if (mesh.boneWeights != nullptr) MemFree(mesh.boneWeights);
  mesh = Mesh{};
}

}  // namespace redgem
