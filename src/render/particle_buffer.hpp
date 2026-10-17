#pragma once

// A frame's particles on the graphics card: their corners, sent anew every frame into room made
// once for as many as can fly, and the two triangles of each, made once.

#include <vector>

#include "render/particle_layout.hpp"

namespace redgem::render_detail {

struct ParticleBuffer {
  unsigned int vertex_array = 0;
  unsigned int corners = 0;
  unsigned int triangles = 0;
};

// `look_location`: the particle shader's vertex attribute of how soft and how lit a particle is.
ParticleBuffer load_particle_buffer(int look_location);
void send_particles(const ParticleBuffer& buffer, const std::vector<render_math::ParticleVertex>& vertices);
// Draws one run of the particles last sent, through the shader in use.
void draw_particle_run(const ParticleBuffer& buffer, const render_math::ParticleRun& run);
void release_particle_buffer(ParticleBuffer& buffer);

}  // namespace redgem::render_detail
