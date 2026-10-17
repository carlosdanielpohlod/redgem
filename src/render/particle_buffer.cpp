#include "render/particle_buffer.hpp"

#include <rlgl.h>

#include <cstddef>
#include <cstdint>

namespace redgem::render_detail {
namespace {

using render_math::ParticleVertex;

constexpr int kTrianglesPerQuad = 2;
constexpr int kIndicesPerQuad = kTrianglesPerQuad * 3;
// A quad's two triangles, by its corners (bottom left, bottom right, top right, top left).
constexpr std::uint16_t kQuadTriangles[kIndicesPerQuad] = {0, 1, 2, 0, 2, 3};
constexpr int kPositionFloats = 3;
constexpr int kUvFloats = 2;
constexpr int kLookFloats = 2;  // soft, lit
constexpr int kStride = static_cast<int>(sizeof(ParticleVertex));
constexpr std::size_t kMaxCorners = kMaxParticles * render_math::kQuadCorners;
static_assert(kMaxCorners <= UINT16_MAX + 1, "the corners of every particle are numbered in 16 bits");

std::vector<std::uint16_t> quad_triangles() {
  std::vector<std::uint16_t> indices;
  indices.reserve(kMaxParticles * kIndicesPerQuad);
  for (std::size_t quad = 0; quad < kMaxParticles; ++quad) {
    const auto first = static_cast<std::uint16_t>(quad * render_math::kQuadCorners);
    for (const std::uint16_t corner : kQuadTriangles) indices.push_back(static_cast<std::uint16_t>(first + corner));
  }
  return indices;
}

void describe(unsigned int location, int floats, int type, bool normalized, std::size_t offset) {
  rlSetVertexAttribute(location, floats, type, normalized, kStride, static_cast<int>(offset));
  rlEnableVertexAttribute(location);
}

// Where the shader finds each part of a corner in the buffer bound now.
void describe_corners(int look_location) {
  describe(RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION, kPositionFloats, RL_FLOAT, false,
           offsetof(ParticleVertex, position));
  describe(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD, kUvFloats, RL_FLOAT, false, offsetof(ParticleVertex, u));
  describe(RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR, render_math::kColorChannels, RL_UNSIGNED_BYTE, true,
           offsetof(ParticleVertex, color));
  describe(static_cast<unsigned int>(look_location), kLookFloats, RL_FLOAT, false, offsetof(ParticleVertex, soft));
}

}  // namespace

ParticleBuffer load_particle_buffer(int look_location) {
  ParticleBuffer buffer;
  buffer.vertex_array = rlLoadVertexArray();
  rlEnableVertexArray(buffer.vertex_array);  // (made, not bound: what follows is kept in it)
  buffer.corners = rlLoadVertexBuffer(nullptr, static_cast<int>(kMaxCorners * sizeof(ParticleVertex)), true);
  describe_corners(look_location);
  const std::vector<std::uint16_t> indices = quad_triangles();
  buffer.triangles =
      rlLoadVertexBufferElement(indices.data(), static_cast<int>(indices.size() * sizeof(std::uint16_t)), false);
  rlDisableVertexArray();
  return buffer;
}

void send_particles(const ParticleBuffer& buffer, const std::vector<ParticleVertex>& vertices) {
  if (vertices.empty()) return;
  rlUpdateVertexBuffer(buffer.corners, vertices.data(), static_cast<int>(vertices.size() * sizeof(ParticleVertex)), 0);
}

void draw_particle_run(const ParticleBuffer& buffer, const render_math::ParticleRun& run) {
  rlEnableVertexArray(buffer.vertex_array);
  rlDrawVertexArrayElements(static_cast<int>(run.first * kIndicesPerQuad),
                            static_cast<int>(run.count * kIndicesPerQuad), nullptr);
  rlDisableVertexArray();
}

void release_particle_buffer(ParticleBuffer& buffer) {
  if (buffer.vertex_array == 0) return;
  rlUnloadVertexArray(buffer.vertex_array);
  rlUnloadVertexBuffer(buffer.corners);
  rlUnloadVertexBuffer(buffer.triangles);
  buffer = ParticleBuffer{};
}

}  // namespace redgem::render_detail
