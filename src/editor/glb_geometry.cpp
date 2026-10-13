#include "editor/glb_geometry.hpp"

#include "scene/scene_config.hpp"
#include "editor/gltf_json.hpp"

namespace redgem::glb_geometry {
namespace {

using namespace gltf_json;

constexpr std::size_t kFloatsPerVec3 = 3;
constexpr int kArrayBufferTarget = 34962;         // glTF bufferView.target for vertex data
constexpr int kElementArrayBufferTarget = 34963;  // ... and for indices
constexpr int kUnsignedIntComponent = 5125;       // glTF componentType UNSIGNED_INT

// object[key] as a size: `missing` when the object has no such key, nullopt when what it has is
// not a whole number of zero or more (a negative offset must never become a huge one).
std::optional<std::size_t> size_at(const nlohmann::json& object, const char* key, std::optional<std::size_t> missing) {
  const auto found = object.find(key);
  if (found == object.end()) return missing;
  return size_from(*found);
}

bool vec3_of_floats(const nlohmann::json& accessor) {
  const auto component = accessor.find(kComponentType);
  const auto type = accessor.find(kType);
  const bool said = component != accessor.end() && type != accessor.end();
  return said && *component == kFloatComponent && *type == kVec3Type;
}

// Whether an accessor holds `float_count` floats as vectors of three.
bool holds_vec3_floats(const nlohmann::json& accessor, std::size_t float_count) {
  if (!vec3_of_floats(accessor)) return false;
  return float_count % kFloatsPerVec3 == 0 && size_at(accessor, kCount, std::nullopt) == float_count / kFloatsPerVec3;
}

// Whether a buffer view is in the file's only buffer, its elements one right after the other.
bool packed_in_only_buffer(const nlohmann::json& view) {
  const std::size_t packed_stride = kFloatsPerVec3 * sizeof(float);
  return size_at(view, kBuffer, std::nullopt) == static_cast<std::size_t>(kOnlyBuffer) &&
         size_at(view, kByteStride, packed_stride) == packed_stride;
}

// Whether `length` bytes at `offset` are inside `whole` bytes. (Compared by subtraction from the
// larger side: sums of what a file says may wrap around.)
bool inside(std::size_t offset, std::size_t length, std::size_t whole) {
  return offset <= whole && length <= whole - offset;
}

using Size = std::optional<std::size_t>;

// The accessor of that number, when it holds `float_count` floats as vectors of three.
const nlohmann::json* vec3_accessor(const nlohmann::json& gltf, std::size_t index, std::size_t float_count) {
  const nlohmann::json* accessor = entry_at(gltf, kAccessors, index);
  if (accessor == nullptr) return nullptr;
  return holds_vec3_floats(*accessor, float_count) ? accessor : nullptr;
}

// The buffer view an accessor reads, when it is packed in the file's only buffer.
const nlohmann::json* packed_view_of(const nlohmann::json& gltf, const nlohmann::json& accessor) {
  const nlohmann::json* view = entry_at(gltf, kBufferViews, accessor.value(kBufferView, nlohmann::json()));
  if (view == nullptr) return nullptr;
  return packed_in_only_buffer(*view) ? view : nullptr;
}

// Where an accessor's `bytes` bytes start in a chunk of `bin_size` bytes, when its view is inside
// the chunk and the bytes inside the view.
Size place_in_chunk(const nlohmann::json& view, const nlohmann::json& accessor, std::size_t bytes,
                    std::size_t bin_size) {
  const Size view_offset = size_at(view, kByteOffset, 0);
  const Size view_length = size_at(view, kByteLength, std::nullopt);
  const Size offset = size_at(accessor, kByteOffset, 0);
  if (!view_offset) return std::nullopt;
  if (!view_length) return std::nullopt;
  if (!offset) return std::nullopt;
  if (!inside(*view_offset, *view_length, bin_size)) return std::nullopt;
  if (!inside(*offset, bytes, *view_length)) return std::nullopt;
  return *view_offset + *offset;
}

// Appends raw bytes to the binary chunk as a new buffer view; returns its index.
int append_buffer_view(GlbFile& glb, const void* data, std::size_t bytes, int target) {
  const std::size_t offset = append_binary(glb, static_cast<const std::uint8_t*>(data), bytes);
  nlohmann::json& views = array_at(glb.json, kBufferViews);
  views.push_back({{kBuffer, kOnlyBuffer}, {kByteOffset, offset}, {kByteLength, bytes}, {kTarget, target}});
  return last_index(views);
}

// Describes a buffer view's elements; returns the accessor's index.
int append_accessor(nlohmann::json& json, int view, int component, std::size_t count, const char* type) {
  nlohmann::json& accessors = array_at(json, kAccessors);
  accessors.push_back({{kBufferView, view}, {kComponentType, component}, {kCount, count}, {kType, type}});
  return last_index(accessors);
}

// One float stream of a mesh (positions, normals, texture coordinates) as a new accessor.
int append_vertex_stream(GlbFile& glb, const std::vector<float>& values, std::size_t vertices, const char* type) {
  const int view = append_buffer_view(glb, values.data(), values.size() * sizeof(float), kArrayBufferTarget);
  return append_accessor(glb.json, view, kFloatComponent, vertices, type);
}

// The vertex streams of a mesh as new accessors; returns the glTF "attributes" naming them.
nlohmann::json append_attributes(GlbFile& glb, const PrimitiveDesc& primitive, const Bounds& bounds) {
  const std::size_t vertices = primitive.positions.size() / config::kFloatsPerPosition;
  const int position = append_vertex_stream(glb, primitive.positions, vertices, kVec3Type);
  write_bounds(glb.json[kAccessors][position], bounds);
  const int normal = append_vertex_stream(glb, primitive.normals, vertices, kVec3Type);
  nlohmann::json attributes = {{kPositionAttribute, position}, {kNormalAttribute, normal}};
  if (!primitive.texcoords.empty()) {
    attributes[kTexcoordAttribute] = append_vertex_stream(glb, primitive.texcoords, vertices, kVec2Type);
  }
  return attributes;
}

int append_indices(GlbFile& glb, const std::vector<std::uint32_t>& indices) {
  const int view =
      append_buffer_view(glb, indices.data(), indices.size() * sizeof(std::uint32_t), kElementArrayBufferTarget);
  return append_accessor(glb.json, view, kUnsignedIntComponent, indices.size(), kScalarType);
}

}  // namespace

std::optional<std::size_t> vec3_stream_offset(const nlohmann::json& gltf, std::size_t accessor_index,
                                              std::size_t float_count, std::size_t bin_size) {
  const nlohmann::json* accessor = vec3_accessor(gltf, accessor_index, float_count);
  if (accessor == nullptr) return std::nullopt;
  const nlohmann::json* view = packed_view_of(gltf, *accessor);
  if (view == nullptr) return std::nullopt;
  return place_in_chunk(*view, *accessor, float_count * sizeof(float), bin_size);
}

void write_bounds(nlohmann::json& accessor, const Bounds& bounds) {
  accessor[kMin] = {bounds.min.x, bounds.min.y, bounds.min.z};
  accessor[kMax] = {bounds.max.x, bounds.max.y, bounds.max.z};
}

std::size_t append_binary(GlbFile& glb, const std::uint8_t* data, std::size_t bytes) {
  glb.bin.resize(glb_padded_size(glb.bin.size()), 0);
  const std::size_t offset = glb.bin.size();
  glb.bin.insert(glb.bin.end(), data, data + bytes);
  return offset;
}

int append_mesh(GlbFile& glb, const std::string& name, const PrimitiveDesc& primitive, const Bounds& bounds) {
  const nlohmann::json attributes = append_attributes(glb, primitive, bounds);
  const int indices = append_indices(glb, primitive.indices);
  nlohmann::json json_primitive = {{kAttributes, attributes}, {kIndices, indices}};
  if (primitive.material != kNoIndex) json_primitive[kMaterial] = primitive.material;
  nlohmann::json& meshes = array_at(glb.json, kMeshes);
  meshes.push_back({{kName, name}, {kPrimitives, nlohmann::json::array({json_primitive})}});
  return last_index(meshes);
}

}  // namespace redgem::glb_geometry
