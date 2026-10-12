#include "render/mesh_builder.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <unordered_map>

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

std::size_t vertex_count_of(const PrimitiveDesc& primitive) {
  return primitive.positions.size() / config::kFloatsPerPosition;
}

// The optional data a primitive carries for every one of its vertices (data of another length
// is as good as none).
struct VertexData {
  bool texcoords = false;
  bool colors = false;
  bool skin = false;
};

VertexData vertex_data_of(const PrimitiveDesc& primitive) {
  const std::size_t vertex_count = vertex_count_of(primitive);
  const std::size_t influences = vertex_count * config::kJointsPerVertex;
  VertexData has;
  has.texcoords = primitive.texcoords.size() == vertex_count * config::kFloatsPerTexcoord;
  has.colors = primitive.colors.size() == vertex_count * config::kChannelsPerColor;
  has.skin = primitive.joints.size() == influences && primitive.weights.size() == influences;
  return has;
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
// has none), a color (white likewise) and, for skinned meshes, its joints and weights (raylib's
// bone attributes).
void copy_vertex_data(Mesh& mesh, const PrimitiveDesc& primitive) {
  const std::size_t texcoord_count = vertex_count_of(primitive) * config::kFloatsPerTexcoord;
  const std::size_t color_count = vertex_count_of(primitive) * config::kChannelsPerColor;
  const VertexData has = vertex_data_of(primitive);
  mesh.texcoords =
      has.texcoords ? copy_array(primitive.texcoords.data(), texcoord_count) : zeroed_array<float>(texcoord_count);
  mesh.colors = has.colors ? copy_array(primitive.colors.data(), color_count) : white_colors(color_count);
  if (!has.skin) return;
  mesh.boneIds = copy_array(primitive.joints.data(), primitive.joints.size());
  mesh.boneWeights = copy_array(primitive.weights.data(), primitive.weights.size());
}

// Appends the `width` values vertex `source` has in `in` to `out`.
template <typename T>
void append_vertex_values(std::vector<T>& out, const std::vector<T>& in, std::uint32_t source, std::size_t width) {
  const auto begin = in.begin() + static_cast<std::ptrdiff_t>(source * width);
  out.insert(out.end(), begin, begin + static_cast<std::ptrdiff_t>(width));
}

// Cuts a primitive into pieces, one triangle at a time: a triangle that would take the piece
// being filled over the limit starts the next one.
class PrimitiveSplitter {
 public:
  PrimitiveSplitter(const PrimitiveDesc& source, std::size_t max_vertices)
      : source_(source), has_(vertex_data_of(source)), max_vertices_(max_vertices) {
    start_piece();
  }

  // Adds the triangle whose three corners start at `first` in the source's indices.
  void add_triangle(std::size_t first) {
    if (remap_.size() + new_vertices(first) > max_vertices_) {
      pieces_.push_back(std::move(piece_));
      start_piece();
    }
    for (std::size_t corner = 0; corner < config::kVerticesPerTriangle; ++corner) {
      const std::uint32_t vertex = source_.indices[first + corner];
      const auto [place, inserted] = remap_.emplace(vertex, static_cast<std::uint32_t>(remap_.size()));
      if (inserted) copy_vertex(vertex);
      piece_.indices.push_back(place->second);
    }
  }

  std::vector<PrimitiveDesc> finish() {
    if (!piece_.indices.empty()) pieces_.push_back(std::move(piece_));
    return std::move(pieces_);
  }

 private:
  void start_piece() {
    piece_ = PrimitiveDesc{};
    piece_.material = source_.material;
    remap_.clear();
  }

  // How many of a triangle's corners the piece being filled does not hold yet.
  std::size_t new_vertices(std::size_t first) const {
    std::size_t count = 0;
    for (std::size_t corner = 0; corner < config::kVerticesPerTriangle; ++corner) {
      count += remap_.count(source_.indices[first + corner]) == 0 ? 1 : 0;
    }
    return count;
  }

  void copy_vertex(std::uint32_t vertex) {
    append_vertex_values(piece_.positions, source_.positions, vertex, config::kFloatsPerPosition);
    append_vertex_values(piece_.normals, source_.normals, vertex, config::kFloatsPerNormal);
    if (has_.texcoords) append_vertex_values(piece_.texcoords, source_.texcoords, vertex, config::kFloatsPerTexcoord);
    if (has_.colors) append_vertex_values(piece_.colors, source_.colors, vertex, config::kChannelsPerColor);
    if (!has_.skin) return;
    append_vertex_values(piece_.joints, source_.joints, vertex, config::kJointsPerVertex);
    append_vertex_values(piece_.weights, source_.weights, vertex, config::kJointsPerVertex);
  }

  const PrimitiveDesc& source_;
  VertexData has_;
  std::size_t max_vertices_;
  std::vector<PrimitiveDesc> pieces_;
  PrimitiveDesc piece_;                                     // the one being filled
  std::unordered_map<std::uint32_t, std::uint32_t> remap_;  // source vertex -> its index in the piece
};

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

std::vector<PrimitiveDesc> split_primitive(const PrimitiveDesc& primitive, std::size_t max_vertices) {
  if (vertex_count_of(primitive) <= max_vertices) return {primitive};
  // A primitive whose indices cannot be trusted is handed back whole: cutting it up would follow
  // them out of its vertices. (build_mesh then says what is wrong with it.)
  if (!primitive_shape_problem(primitive).empty()) return {primitive};
  PrimitiveSplitter splitter(primitive, max_vertices);
  for (std::size_t first = 0; first + config::kVerticesPerTriangle <= primitive.indices.size();
       first += config::kVerticesPerTriangle) {
    splitter.add_triangle(first);
  }
  return splitter.finish();
}

std::vector<PrimitiveDesc> mesh_pieces(const MeshDesc& mesh, std::size_t max_vertices) {
  std::vector<PrimitiveDesc> pieces;
  for (const PrimitiveDesc& primitive : mesh.primitives) {
    std::vector<PrimitiveDesc> split = split_primitive(primitive, max_vertices);
    pieces.insert(pieces.end(), std::make_move_iterator(split.begin()), std::make_move_iterator(split.end()));
  }
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
