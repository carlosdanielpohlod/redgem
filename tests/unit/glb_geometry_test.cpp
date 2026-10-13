#include <doctest/doctest.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "editor/glb_geometry.hpp"
#include "editor/gltf_json.hpp"

using namespace redgem;
using namespace redgem::gltf_json;
using namespace redgem::glb_geometry;
using nlohmann::json;

namespace {

constexpr std::size_t kStreamFloats = 9;  // three vertices
constexpr std::size_t kStreamBytes = kStreamFloats * sizeof(float);
constexpr std::size_t kViewAt = 64;  // bytes into the binary chunk
constexpr std::size_t kChunkBytes = 256;
constexpr std::size_t kChunkAlignment = 4;  // bytes: what a .glb pads its chunks to
constexpr std::uint8_t kFiller = 0xAB;
constexpr std::uint8_t kSomeBytes[] = {1, 2, 3, 4, 5, 6, 7};  // one byte past an aligned size
constexpr int kTriangleVertices = 3;
constexpr int kTriangleIndices = 3;
constexpr int kMaterialIndex = 2;
constexpr Vec3 kLow{-1.0f, 0.0f, -2.0f};
constexpr Vec3 kHigh{1.0f, 3.0f, 2.0f};

// A file with one accessor of three packed positions in a buffer view at kViewAt.
json positions_file() {
  return {{kAccessors, {{{kBufferView, 0}, {kComponentType, kFloatComponent}, {kCount, 3}, {kType, kVec3Type}}}},
          {kBufferViews, {{{kBuffer, 0}, {kByteOffset, kViewAt}, {kByteLength, kStreamBytes}}}}};
}

std::optional<std::size_t> offset_in(const json& file) {
  return vec3_stream_offset(file, 0, kStreamFloats, kChunkBytes);
}

// One triangle, with normals, texture coordinates and a material.
PrimitiveDesc triangle() {
  PrimitiveDesc primitive;
  primitive.positions = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
  primitive.normals = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f};
  primitive.texcoords = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f};
  primitive.indices = {0, 1, 2};
  primitive.material = kMaterialIndex;
  return primitive;
}

}  // namespace

TEST_CASE("vertices are written back only where the file really keeps them") {
  CHECK(offset_in(positions_file()) == kViewAt);
  json with_accessor_offset = positions_file();
  with_accessor_offset[kBufferViews][0][kByteLength] = kStreamBytes + 4;
  with_accessor_offset[kAccessors][0][kByteOffset] = 4;
  CHECK(offset_in(with_accessor_offset) == kViewAt + 4);
  CHECK_FALSE(vec3_stream_offset(positions_file(), 1, kStreamFloats, kChunkBytes).has_value());
  CHECK_FALSE(vec3_stream_offset(positions_file(), 0, kStreamFloats + 3, kChunkBytes).has_value());
  CHECK_FALSE(vec3_stream_offset(positions_file(), 0, kStreamFloats, kViewAt + kStreamBytes - 1).has_value());
}

TEST_CASE("an offset before the start of the data is refused instead of becoming a huge one") {
  for (const char* owner : {kBufferViews, kAccessors}) {
    for (const json& offset : {json(-8), json(-1), json("8"), json(8.5), json(nullptr)}) {
      CAPTURE(owner);
      CAPTURE(offset.dump());
      json file = positions_file();
      file[owner][0][kByteOffset] = offset;
      CHECK_FALSE(offset_in(file).has_value());
    }
  }
}

TEST_CASE("vertices are not written into data kept any other way than the editor keeps it") {
  const std::pair<const char*, json> changes[] = {
      {kComponentType, 5123}, {kType, kVec2Type}, {kType, 3},        {kCount, 4},
      {kCount, -3},           {kBufferView, 5},   {kBufferView, -1}, {kBufferView, "0"}};
  for (const auto& [key, value] : changes) {
    CAPTURE(key);
    CAPTURE(value.dump());
    json file = positions_file();
    file[kAccessors][0][key] = value;
    CHECK_FALSE(offset_in(file).has_value());
  }
  const std::pair<const char*, json> view_changes[] = {
      {kBuffer, 1}, {kByteStride, 16}, {kByteLength, kStreamBytes - 1}, {kByteLength, -1}, {kByteOffset, kChunkBytes}};
  for (const auto& [key, value] : view_changes) {
    CAPTURE(key);
    CAPTURE(value.dump());
    json file = positions_file();
    file[kBufferViews][0][key] = value;
    CHECK_FALSE(offset_in(file).has_value());
  }
  json no_buffer = positions_file();
  no_buffer[kBufferViews][0].erase(kBuffer);
  CHECK_FALSE(offset_in(no_buffer).has_value());
  CHECK_FALSE(offset_in(json::array()).has_value());
  CHECK_FALSE(offset_in(json{{kAccessors, 3}}).has_value());
}

TEST_CASE("bytes appended to the binary chunk start on a multiple of four, after what was there") {
  GlbFile glb;
  glb.bin.assign(1, kFiller);
  CHECK(append_binary(glb, kSomeBytes, sizeof(kSomeBytes)) == kChunkAlignment);
  CHECK(glb.bin.size() == kChunkAlignment + sizeof(kSomeBytes));
  CHECK(glb.bin[0] == kFiller);
  CHECK(glb.bin[kChunkAlignment] == kSomeBytes[0]);
  const std::size_t next = append_binary(glb, kSomeBytes, sizeof(kSomeBytes));
  CHECK(next % kChunkAlignment == 0);
  CHECK(next >= kChunkAlignment + sizeof(kSomeBytes));
}

TEST_CASE("a shape made in the editor becomes a mesh whose streams the file can find again") {
  GlbFile glb;
  const int mesh = append_mesh(glb, "Triangle", triangle(), Bounds{kLow, kHigh});
  CHECK(mesh == 0);
  const json& primitive = glb.json[kMeshes][0][kPrimitives][0];
  CHECK(glb.json[kMeshes][0][kName] == "Triangle");
  CHECK(primitive[kMaterial] == kMaterialIndex);
  const std::size_t positions = primitive[kAttributes][kPositionAttribute].get<std::size_t>();
  const std::size_t normals = primitive[kAttributes][kNormalAttribute].get<std::size_t>();
  const json& position_accessor = glb.json[kAccessors][positions];
  CHECK(position_accessor[kCount] == kTriangleVertices);
  CHECK(position_accessor[kType] == kVec3Type);
  CHECK(vec3_stream_offset(glb.json, positions, kStreamFloats, glb.bin.size()).has_value());
  CHECK(vec3_stream_offset(glb.json, normals, kStreamFloats, glb.bin.size()).has_value());
  CHECK(glb.json[kAccessors][primitive[kAttributes][kTexcoordAttribute].get<std::size_t>()][kType] == kVec2Type);
  CHECK(glb.json[kAccessors][primitive[kIndices].get<std::size_t>()][kCount] == kTriangleIndices);
  CHECK(position_accessor[kMin] == json({kLow.x, kLow.y, kLow.z}));
  CHECK(position_accessor[kMax] == json({kHigh.x, kHigh.y, kHigh.z}));
}

TEST_CASE("a mesh without a material or texture coordinates is written without them") {
  GlbFile glb;
  PrimitiveDesc bare = triangle();
  bare.texcoords.clear();
  bare.material = kNoIndex;
  append_mesh(glb, "Bare", bare, Bounds{kLow, kHigh});
  const json& primitive = glb.json[kMeshes][0][kPrimitives][0];
  CHECK_FALSE(primitive.contains(kMaterial));
  CHECK_FALSE(primitive[kAttributes].contains(kTexcoordAttribute));
  CHECK(append_mesh(glb, "Second", bare, Bounds{kLow, kHigh}) == 1);
}
