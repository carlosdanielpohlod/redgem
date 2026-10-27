#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/math.hpp"
#include "scene/particle_look.hpp"

namespace redgem {

// The most particles that fly at once: a burst past that lets out fewer.
inline constexpr std::size_t kMaxParticles = 4096;

// What a burst is like when nothing else is asked for.
inline constexpr int kDefaultBurstCount = 12;
inline constexpr float kDefaultBurstSpeed = 3.0f;  // meters per second
inline constexpr float kDefaultBurstLife = 0.8f;   // seconds
inline constexpr float kDefaultBurstSize = 0.12f;  // meters across

// A puff of particles let out at once (Particles.burst in Ruby): sparks, dust, smoke, confetti.
struct ParticleBurst {
  Vec3 position;
  int count = kDefaultBurstCount;
  // Where they fly, and how far from that they may stray: 0 is a jet, 180 every direction.
  Vec3 direction = Vec3::up();
  float spread_degrees = kDegreesPerHalfTurn;
  float speed = kDefaultBurstSpeed;  // meters per second (each particle gets a little more or less)
  float life = kDefaultBurstLife;    // seconds (also varied a little)
  float gravity = 1.0f;              // share of the world's gravity: 0 floats, negative rises (smoke)
  // How each one looks (its size also varies a little); its picture is the one named by `image`.
  ParticleLook look = fading_look(kDefaultBurstSize);
  // A picture from the game's images/ folder drawn on each particle (a puff of smoke, a star, a
  // sprite sheet of flames); empty for a soft round dot.
  std::string image;
  float spin_degrees = 0.0f;  // how fast each one turns, per second (half of them the other way)
};

struct Particle {
  Vec3 position;
  Vec3 velocity;
  float age = 0.0f;
  float life = 0.0f;
  float gravity = 0.0f;
  ParticleLook look;
  float angle = 0.0f;  // radians it is turned by, around the line of sight
  float spin = 0.0f;   // radians per second

  // How much of its life is over: 0 at birth, 1 at the end.
  float lived() const { return life > 0.0f ? age / life : 1.0f; }
  // What is drawn now.
  float size_now() const { return look.size.at(lived()); }
  Vec3 color_now() const { return look.color.at(lived()); }
  float alpha_now() const { return look.alpha.at(lived()); }
  int frame_now() const { return flipbook_frame(look.flipbook, age, life); }
};

// The directions a burst's particles fly in: a cone around `axis`, which they lean away from
// along `side` and `other` (both across it).
struct BurstCone {
  Vec3 axis;
  Vec3 side;
  Vec3 other;
  float widest = 0.0f;  // cosine of the angle between the axis and the cone's edge
};
BurstCone burst_cone(Vec3 direction, float spread_degrees);
// One direction of a cone, picked by two numbers from 0 to 1 (how far from the axis, and where
// around it), so that evenly spread numbers give directions evenly spread over the cone.
Vec3 heading_in(const BurstCone& cone, float along_unit, float around_unit);

// Every particle of a running game. They are looks only (nothing collides with them), and the same
// calls always give the same particles, so tests and recordings repeat exactly.
class ParticleSystem {
 public:
  // Adds a burst; particles over the limit are left out. Returns how many were added.
  int burst(const ParticleBurst& burst);
  void update(float dt);
  void clear();
  void set_seed(std::uint32_t seed) { seed_ = seed; }
  void set_gravity(Vec3 gravity) { gravity_ = gravity; }
  const std::vector<Particle>& particles() const { return particles_; }
  // The names of the pictures particles were given so far (Particle::image points here).
  const std::vector<std::string>& images() const { return images_; }

 private:
  friend class ParticleEmitters;
  struct PreparedBurst {
    BurstCone cone;
    int image = kNoParticleImage;
  };
  PreparedBurst prepare(const ParticleBurst& burst);
  bool emit_prepared(const ParticleBurst& burst, const PreparedBurst& prepared, Vec3 position, float age);
  float random_unit();  // 0 to 1
  int image_index(const std::string& name);
  Particle new_particle(const ParticleBurst& burst, const BurstCone& cone, int image);
  // Pictures start turned any which way, or they would all look like the same stamp.
  void turn_at_random(Particle& particle, float spin);

  std::vector<Particle> particles_;
  std::vector<std::string> images_;
  Vec3 gravity_;
  std::uint32_t seed_ = 1;
};

}  // namespace redgem
