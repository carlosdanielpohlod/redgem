#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/math.hpp"
#include "scene/scene_desc.hpp"

namespace redgem {

// A puff of particles let out at once (Particles.burst in Ruby): sparks, dust, smoke, confetti.
struct ParticleBurst {
  Vec3 position;
  int count = 12;
  // Where they fly, and how far from that they may stray: 0 is a jet, 180 every direction.
  Vec3 direction = Vec3::up();
  float spread_degrees = 180.0f;
  float speed = 3.0f;    // meters per second (each particle gets a little more or less)
  float life = 0.8f;     // seconds (also varied a little)
  float size = 0.12f;    // meters across
  float gravity = 1.0f;  // share of the world's gravity: 0 floats, negative rises (smoke)
  Color color;
  bool glow = false;     // lit from within: adds light instead of covering what is behind
  // A picture from the game's images/ folder drawn on each particle (a puff of smoke, a star);
  // empty for a soft round dot.
  std::string image;
  float spin_degrees = 0.0f;  // how fast each one turns, per second (half of them the other way)
};

inline constexpr int kNoParticleImage = -1;

struct Particle {
  Vec3 position;
  Vec3 velocity;
  float age = 0.0f;
  float life = 0.0f;
  float size = 0.0f;
  float gravity = 0.0f;
  Color color;
  bool glow = false;
  int image = kNoParticleImage;  // index into ParticleSystem::images()
  float angle = 0.0f;            // radians it is turned by, around the line of sight
  float spin = 0.0f;             // radians per second

  // What is drawn: particles shrink and fade out over the last part of their life.
  float size_now() const;
  float alpha_now() const;
};

// Every particle of a running game. They are looks only (nothing collides with them), and the same
// calls always give the same particles, so tests and recordings repeat exactly.
class ParticleSystem {
 public:
  // Adds a burst; particles over the limit are left out. Returns how many were added.
  int burst(const ParticleBurst& burst);
  void update(float dt);
  void clear();
  void set_gravity(Vec3 gravity) { gravity_ = gravity; }
  const std::vector<Particle>& particles() const { return particles_; }
  // The names of the pictures particles were given so far (Particle::image points here).
  const std::vector<std::string>& images() const { return images_; }

 private:
  float random_unit();  // 0 to 1
  int image_index(const std::string& name);

  std::vector<Particle> particles_;
  std::vector<std::string> images_;
  Vec3 gravity_;
  std::uint32_t seed_ = 1;
};

}  // namespace redgem
