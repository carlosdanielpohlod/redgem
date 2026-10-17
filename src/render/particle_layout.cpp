#include "render/particle_layout.hpp"

#include <algorithm>

namespace redgem::render_math {
namespace {

constexpr float kMaxColorByte = 255.0f;
constexpr std::size_t kLayers = 2;  // the particles that cover what is behind them, and the ones that glow

// The run a particle goes to: its layer's runs come one per batch.
std::size_t run_key(const Particle& particle, const std::vector<std::size_t>& batch_of, std::size_t batches) {
  const std::size_t batch =
      particle.look.image == kNoParticleImage ? 0 : batch_of[static_cast<std::size_t>(particle.look.image)];
  return (particle.look.glow ? batches : 0) + batch;
}

std::uint8_t color_byte(float channel) {
  return static_cast<std::uint8_t>(std::clamp(channel, 0.0f, 1.0f) * kMaxColorByte);
}

// The part of its picture a particle shows: the whole of it, or a cell of its sprite sheet.
UvCell cell_of(const Particle& particle) {
  if (particle.look.image == kNoParticleImage) return UvCell{};
  return flipbook_cell(particle.look.flipbook, particle.frame_now());
}

// A particle's four corners, from `out` on.
void write_quad(const Particle& particle, const Facing& view, ParticleVertex* out) {
  const auto corners = particle_corners(particle.position, particle.size_now(), particle.angle, view);
  const UvCell cell = cell_of(particle);
  const Vec3 color = particle.color_now();
  ParticleVertex vertex;
  vertex.color = {color_byte(color.x), color_byte(color.y), color_byte(color.z), color_byte(particle.alpha_now())};
  vertex.soft = particle.look.soft;
  vertex.lit = particle.look.takes_light() ? kOn : kOff;
  for (int corner = 0; corner < kQuadCorners; ++corner) {
    vertex.position = corners[corner];
    vertex.u = cell.left + kQuadU[corner] * (cell.right - cell.left);
    vertex.v = cell.top + kQuadV[corner] * (cell.bottom - cell.top);
    out[corner] = vertex;
  }
}

// How many particles each run gets, and whether any of them is soft or glows.
void count_runs(const std::vector<Particle>& particles, const std::vector<std::size_t>& batch_of, std::size_t batches,
                ParticleLayout& layout) {
  layout.counts.assign(batches * kLayers, 0);
  layout.any_soft = false;
  layout.any_glow = false;
  for (const Particle& particle : particles) {
    ++layout.counts[run_key(particle, batch_of, batches)];
    layout.any_soft = layout.any_soft || particle.look.soft > 0.0f;
    layout.any_glow = layout.any_glow || particle.look.glow;
  }
}

// The runs that get any particles, and where each one's first goes.
void open_runs(std::size_t batches, ParticleLayout& layout) {
  layout.runs.clear();
  layout.next.resize(layout.counts.size());
  std::size_t first = 0;
  for (std::size_t key = 0; key < layout.counts.size(); ++key) {
    layout.next[key] = first;
    if (layout.counts[key] == 0) continue;
    layout.runs.push_back({key % batches, key >= batches, first, layout.counts[key]});
    first += layout.counts[key];
  }
}

}  // namespace

UvCell flipbook_cell(const Flipbook& book, int frame) {
  const float width = 1.0f / static_cast<float>(book.columns);
  const float height = 1.0f / static_cast<float>(book.rows);
  const int row = frame / book.columns;
  const int column = frame % book.columns;
  const float left = static_cast<float>(column) * width;
  const float top = static_cast<float>(row) * height;
  return {left, top, left + width, top + height};
}

void lay_out_particles(const std::vector<Particle>& particles, const std::vector<std::size_t>& batch_of,
                       std::size_t batches, const Facing& view, ParticleLayout& layout) {
  count_runs(particles, batch_of, batches, layout);
  open_runs(batches, layout);
  layout.vertices.resize(particles.size() * kQuadCorners);
  for (const Particle& particle : particles) {
    const std::size_t place = layout.next[run_key(particle, batch_of, batches)]++;
    write_quad(particle, view, &layout.vertices[place * kQuadCorners]);
  }
}

}  // namespace redgem::render_math
