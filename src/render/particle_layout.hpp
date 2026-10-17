#pragma once

// Particles as the particle shader draws them: four corners each, gathered into runs that share a
// picture and a way of mixing with what is behind them. No window and no graphics card needed, so
// it can be tested.

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/math.hpp"
#include "render/render_math.hpp"
#include "scene/particles.hpp"

namespace redgem::render_math {

// One corner of a particle, as the particle shader takes it.
struct ParticleVertex {
  Vec3 position;  // in the world
  float u = 0.0f;
  float v = 0.0f;
  // Red, green, blue and alpha, 0 to 255 as color pickers show them (raylib's batches took the
  // same bytes, made the same way).
  std::array<std::uint8_t, kColorChannels> color{};
  float soft = 0.0f;  // meters it fades over where it meets what is behind it
  float lit = kOff;   // kOn when it takes the light around it
};

// Where in its picture a cell of a sprite sheet is, in texture coordinates (0, 0 the top left).
struct UvCell {
  float left = 0.0f;
  float top = 0.0f;
  float right = 1.0f;
  float bottom = 1.0f;
};
UvCell flipbook_cell(const Flipbook& book, int frame);

// The particles drawn with one batch's picture, glowing or not: `count` of them from the
// `first` on, four corners each.
struct ParticleRun {
  std::size_t batch = 0;
  bool glow = false;
  std::size_t first = 0;
  std::size_t count = 0;
};

// What a frame's particles are drawn from, kept from frame to frame so that it is not made anew.
struct ParticleLayout {
  std::vector<ParticleVertex> vertices;
  std::vector<ParticleRun> runs;    // the ones that cover what is behind them first, then the ones that glow
  std::vector<std::size_t> counts;  // how many particles each run gets (one run per batch and layer)
  std::vector<std::size_t> next;    // where the next particle of each run goes
  bool any_soft = false;            // whether any of them fades into what is behind it
  bool any_glow = false;            // whether any of them glows
};

// Lays particles out run by run, in the order they came within each. `batch_of`: the batch of the
// picture of each of the system's images (Particle::look.image); plain dots are batch 0.
void lay_out_particles(const std::vector<Particle>& particles, const std::vector<std::size_t>& batch_of,
                       std::size_t batches, const Facing& view, ParticleLayout& layout);

}  // namespace redgem::render_math
