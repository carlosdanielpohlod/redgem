#include <doctest/doctest.h>

#include <vector>

#include "render/particle_layout.hpp"

using namespace redgem;
using namespace redgem::render_math;

namespace {

constexpr std::size_t kTwoBatches = 2;  // the plain dots, and one picture
const Facing kFacingForward{Vec3::right(), Vec3::up()};

Particle particle_wearing(int image, bool glow) {
  Particle particle;
  particle.life = 1.0f;
  particle.look.size = steady(1.0f);
  particle.look.image = image;
  particle.look.glow = glow;
  return particle;
}

}  // namespace

TEST_CASE("a cell of a sprite sheet is read like a page, from the top left") {
  const Flipbook sheet{4, 2, 0.0f};
  const UvCell first = flipbook_cell(sheet, 0);
  CHECK(first.left == doctest::Approx(0.0f));
  CHECK(first.top == doctest::Approx(0.0f));
  CHECK(first.right == doctest::Approx(0.25f));
  CHECK(first.bottom == doctest::Approx(0.5f));
  const UvCell sixth = flipbook_cell(sheet, 5);  // second row, second column
  CHECK(sixth.left == doctest::Approx(0.25f));
  CHECK(sixth.top == doctest::Approx(0.5f));
  CHECK(sixth.bottom == doctest::Approx(1.0f));
  const UvCell whole = flipbook_cell(Flipbook{}, 0);
  CHECK(whole.right == doctest::Approx(1.0f));
  CHECK(whole.bottom == doctest::Approx(1.0f));
}

TEST_CASE("particles are laid out in runs: those that cover first, by picture, then those that glow") {
  const std::vector<Particle> particles = {particle_wearing(0, true), particle_wearing(kNoParticleImage, false),
                                           particle_wearing(0, false), particle_wearing(kNoParticleImage, false)};
  const std::vector<std::size_t> batch_of = {1};  // the one image is the second batch
  ParticleLayout layout;
  lay_out_particles(particles, batch_of, kTwoBatches, kFacingForward, layout);
  REQUIRE(layout.runs.size() == 3);
  CHECK(layout.runs[0].batch == 0);  // the two plain dots
  CHECK(layout.runs[0].count == 2);
  CHECK_FALSE(layout.runs[0].glow);
  CHECK(layout.runs[1].batch == 1);
  CHECK(layout.runs[1].first == 2);
  CHECK_FALSE(layout.runs[1].glow);
  CHECK(layout.runs[2].batch == 1);
  CHECK(layout.runs[2].glow);
  CHECK(layout.runs[2].first == 3);
  CHECK(layout.vertices.size() == particles.size() * kQuadCorners);
  CHECK_FALSE(layout.any_soft);
}

TEST_CASE("a particle's corners carry its color, its cell of the sheet, and how soft and lit it is") {
  Particle flame = particle_wearing(0, false);
  flame.look.color = steady(Vec3{1.0f, kHalf, 0.0f});
  flame.look.alpha = steady(1.0f);
  flame.look.flipbook = Flipbook{2, 2, 0.0f};
  flame.age = 0.8f;  // the last of four cells: bottom right
  flame.look.soft = 0.5f;
  flame.look.lit = true;
  ParticleLayout layout;
  lay_out_particles({flame}, {1}, kTwoBatches, kFacingForward, layout);
  REQUIRE(layout.vertices.size() == kQuadCorners);
  const ParticleVertex& bottom_left = layout.vertices[0];
  CHECK(bottom_left.color[0] == 255);
  CHECK(bottom_left.color[1] == 127);  // truncated, as raylib's batches made their bytes
  CHECK(bottom_left.color[3] == 255);
  CHECK(bottom_left.u == doctest::Approx(kHalf));
  CHECK(bottom_left.v == doctest::Approx(1.0f));
  CHECK(layout.vertices[2].u == doctest::Approx(1.0f));  // top right
  CHECK(layout.vertices[2].v == doctest::Approx(kHalf));
  CHECK(bottom_left.soft == doctest::Approx(0.5f));
  CHECK(bottom_left.lit == doctest::Approx(kOn));
  CHECK(layout.any_soft);
}

TEST_CASE("the same layout is used again, frame after frame, whatever came before") {
  ParticleLayout layout;
  lay_out_particles({particle_wearing(0, true), particle_wearing(0, true)}, {1}, kTwoBatches, kFacingForward, layout);
  lay_out_particles({particle_wearing(kNoParticleImage, false)}, {1}, kTwoBatches, kFacingForward, layout);
  REQUIRE(layout.runs.size() == 1);
  CHECK(layout.runs[0].count == 1);
  CHECK(layout.vertices.size() == kQuadCorners);
  lay_out_particles({}, {1}, kTwoBatches, kFacingForward, layout);
  CHECK(layout.runs.empty());
}
