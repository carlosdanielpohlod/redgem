// What the loader takes out of a glTF accessor. Every accessor here is made by the test, over
// values of its own.

#include <doctest/doctest.h>

#include <cstdint>
#include <utility>
#include <vector>

#include "scene/gltf/gltf_accessors.hpp"

using namespace redgem;

namespace {

constexpr std::uint8_t kFullByte = 255;
constexpr std::uint8_t kHalfByte = 127;  // half of a full channel, rounded down
constexpr std::uint16_t kJointPastAByte = 300;
constexpr std::size_t kMoreThanStored = 100;  // elements an accessor claims without having them

// An accessor over `values`, which it keeps: `per_element` of them make one element.
template <typename T>
struct Stored {
  std::vector<T> values;
  cgltf_buffer buffer{};
  cgltf_buffer_view view{};
  cgltf_accessor accessor{};

  Stored(std::vector<T> kept, cgltf_component_type component, cgltf_type type) : values(std::move(kept)) {
    buffer.data = values.data();
    buffer.size = values.size() * sizeof(T);
    view.buffer = &buffer;
    view.size = buffer.size;
    accessor.buffer_view = &view;
    accessor.component_type = component;
    accessor.type = type;
    accessor.stride = cgltf_num_components(type) * sizeof(T);
    accessor.count = values.size() / cgltf_num_components(type);
  }
  Stored(const Stored&) = delete;
  Stored& operator=(const Stored&) = delete;
};

Stored<float> floats(std::vector<float> values, cgltf_type type) {
  return {std::move(values), cgltf_component_type_r_32f, type};
}

}  // namespace

TEST_CASE("an accessor's floats are every component of every element") {
  const Stored<float> positions = floats({1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f}, cgltf_type_vec3);
  CHECK(gltf_accessors::unpack_floats(&positions.accessor) == positions.values);
}

TEST_CASE("an accessor that is missing, or claims more than its data holds, gives nothing") {
  CHECK(gltf_accessors::unpack_floats(nullptr).empty());
  CHECK(gltf_accessors::joint_bytes(nullptr).empty());
  CHECK(gltf_accessors::rgba_bytes(nullptr).empty());

  Stored<float> lying = floats({1.0f, 2.0f, 3.0f}, cgltf_type_vec3);
  lying.accessor.count = kMoreThanStored;
  CHECK(gltf_accessors::unpack_floats(&lying.accessor).empty());
  CHECK(gltf_accessors::rgba_bytes(&lying.accessor).empty());
  CHECK(gltf_accessors::triangle_corners(&lying.accessor, 3).empty());
}

TEST_CASE("joints are four bytes a vertex, and an index past a byte becomes the highest a byte holds") {
  const Stored<std::uint16_t> joints({0, 1, kJointPastAByte, 2, 3, 0, 0, 0}, cgltf_component_type_r_16u,
                                     cgltf_type_vec4);
  CHECK(gltf_accessors::joint_bytes(&joints.accessor) == std::vector<std::uint8_t>{0, 1, kFullByte, 2, 3, 0, 0, 0});
}

TEST_CASE("colors without an alpha are opaque, and channels are brought into what a byte holds") {
  const Stored<float> rgb = floats({1.0f, 0.5f, 0.0f, 2.0f, -1.0f, 0.0f}, cgltf_type_vec3);
  CHECK(gltf_accessors::rgba_bytes(&rgb.accessor) ==
        std::vector<std::uint8_t>{kFullByte, kHalfByte, 0, kFullByte, kFullByte, 0, 0, kFullByte});

  const Stored<float> rgba = floats({0.0f, 1.0f, 0.0f, 0.5f}, cgltf_type_vec4);
  CHECK(gltf_accessors::rgba_bytes(&rgba.accessor) == std::vector<std::uint8_t>{0, kFullByte, 0, kHalfByte});
}

TEST_CASE("a mesh's corners are the ones it lists, or every vertex in turn when it lists none") {
  const Stored<std::uint16_t> listed({2, 1, 0, 0, 1, 3}, cgltf_component_type_r_16u, cgltf_type_scalar);
  CHECK(gltf_accessors::triangle_corners(&listed.accessor, 4) == std::vector<std::uint32_t>{2, 1, 0, 0, 1, 3});
  CHECK(gltf_accessors::triangle_corners(nullptr, 3) == std::vector<std::uint32_t>{0, 1, 2});
  CHECK(gltf_accessors::triangle_corners(nullptr, 0).empty());
}
