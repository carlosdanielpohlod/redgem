#pragma once

// A mesh being put together vertex by vertex: what the built-in shapes are made with.

#include <cstdint>

#include "scene/scene_config.hpp"
#include "scene/scene_desc.hpp"

namespace redgem {

struct PrimitiveBuilder {
  PrimitiveDesc out;

  // The index the next vertex will get.
  std::uint32_t next_index() const {
    return static_cast<std::uint32_t>(out.positions.size() / config::kFloatsPerPosition);
  }
  // Adds a vertex and returns its index.
  std::uint32_t vertex(Vec3 position, Vec3 normal, float u, float v) {
    out.positions.insert(out.positions.end(), {position.x, position.y, position.z});
    out.normals.insert(out.normals.end(), {normal.x, normal.y, normal.z});
    out.texcoords.insert(out.texcoords.end(), {u, v});
    return next_index() - 1;
  }
  // Counter-clockwise when seen from the side the normal points to.
  void triangle(std::uint32_t a, std::uint32_t b, std::uint32_t c) { out.indices.insert(out.indices.end(), {a, b, c}); }
  void quad(std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
    triangle(a, b, c);
    triangle(a, c, d);
  }
  // A quad whose four corners were added one after the other, `first` first.
  void quad_from(std::uint32_t first) {
    const std::uint32_t second = first + 1;
    const std::uint32_t third = second + 1;
    quad(first, second, third, third + 1);
  }
};

}  // namespace redgem
