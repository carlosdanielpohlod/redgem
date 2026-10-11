#include "scene/particles.hpp"

#include <algorithm>
#include <cmath>

namespace redgem {

namespace {

constexpr std::size_t kMaxParticles = 4096;
constexpr float kSpeedVariation = 0.4f;  // each particle's speed differs from the burst's by up to this share
constexpr float kLifeVariation = 0.25f;
constexpr float kSizeVariation = 0.3f;
constexpr float kSpinVariation = 0.5f;
constexpr float kFadeStart = 0.5f;  // share of its life after which a particle fades and shrinks
constexpr float kEndSize = 0.3f;    // share of its size left at the end
constexpr float kAirDrag = 0.6f;    // share of its speed a particle loses per second
// A linear congruential generator (Numerical Recipes' constants), and how many of its bits are used.
constexpr std::uint32_t kRandomMultiplier = 1664525u;
constexpr std::uint32_t kRandomIncrement = 1013904223u;
constexpr int kRandomShift = 8;
constexpr float kRandomRange = 16777216.0f;  // 2^24

float varied(float value, float variation, float unit) { return value * (1.0f + variation * (unit * kTwo - 1.0f)); }

float fade(const Particle& particle) {
  const float lived = particle.life > 0.0f ? particle.age / particle.life : 1.0f;
  return std::clamp((lived - kFadeStart) / (1.0f - kFadeStart), 0.0f, 1.0f);
}

}  // namespace

float Particle::size_now() const { return size * (1.0f - (1.0f - kEndSize) * fade(*this)); }
float Particle::alpha_now() const { return color.a * (1.0f - fade(*this)); }

float ParticleSystem::random_unit() {
  seed_ = seed_ * kRandomMultiplier + kRandomIncrement;
  return static_cast<float>(seed_ >> kRandomShift) / kRandomRange;
}

int ParticleSystem::image_index(const std::string& name) {
  if (name.empty()) return kNoParticleImage;
  const auto found = std::find(images_.begin(), images_.end(), name);
  if (found != images_.end()) return static_cast<int>(found - images_.begin());
  images_.push_back(name);
  return static_cast<int>(images_.size()) - 1;
}

BurstCone burst_cone(Vec3 direction, float spread_degrees) {
  BurstCone cone;
  cone.axis = length(direction) > kEpsilon ? normalize(direction) : Vec3::up();
  // Two directions across the axis, to lean away from it.
  const Vec3 helper = std::fabs(cone.axis.y) < kHalf ? Vec3::up() : Vec3::right();
  cone.side = normalize(cross(cone.axis, helper));
  cone.other = cross(cone.axis, cone.side);
  cone.widest = std::cos(degrees_to_radians(std::clamp(spread_degrees, 0.0f, kDegreesPerHalfTurn)));
  return cone;
}

Vec3 heading_in(const BurstCone& cone, float along_unit, float around_unit) {
  // Evenly over the cap of the sphere within the spread.
  const float along = 1.0f - along_unit * (1.0f - cone.widest);
  const float across = std::sqrt(std::max(0.0f, 1.0f - along * along));
  const float around = around_unit * kTwo * kPi;
  return cone.axis * along + (cone.side * std::cos(around) + cone.other * std::sin(around)) * across;
}

void ParticleSystem::turn_at_random(Particle& particle, float spin) {
  particle.angle = random_unit() * kTwo * kPi;
  const float speed = varied(spin, kSpinVariation, random_unit());
  particle.spin = speed * (random_unit() < kHalf ? -1.0f : 1.0f);
}

Particle ParticleSystem::new_particle(const ParticleBurst& burst, const BurstCone& cone, int image) {
  const float along_unit = random_unit();
  const Vec3 heading = heading_in(cone, along_unit, random_unit());
  Particle particle;
  particle.position = burst.position;
  particle.velocity = heading * varied(burst.speed, kSpeedVariation, random_unit());
  particle.life = varied(burst.life, kLifeVariation, random_unit());
  particle.size = varied(burst.size, kSizeVariation, random_unit());
  particle.gravity = burst.gravity;
  particle.color = burst.color;
  particle.glow = burst.glow;
  particle.image = image;
  if (image != kNoParticleImage) turn_at_random(particle, degrees_to_radians(burst.spin_degrees));
  return particle;
}

int ParticleSystem::burst(const ParticleBurst& burst) {
  const int image = image_index(burst.image);
  const BurstCone cone = burst_cone(burst.direction, burst.spread_degrees);
  int added = 0;
  for (; added < burst.count && particles_.size() < kMaxParticles; ++added) {
    particles_.push_back(new_particle(burst, cone, image));
  }
  return added;
}

void ParticleSystem::update(float dt) {
  if (dt <= 0.0f) return;
  const float kept_speed = std::max(0.0f, 1.0f - kAirDrag * dt);
  for (Particle& particle : particles_) {
    particle.age += dt;
    particle.angle += particle.spin * dt;
    particle.velocity = (particle.velocity + gravity_ * (particle.gravity * dt)) * kept_speed;
    particle.position = particle.position + particle.velocity * dt;
  }
  particles_.erase(std::remove_if(particles_.begin(), particles_.end(),
                                  [](const Particle& particle) { return particle.age >= particle.life; }),
                   particles_.end());
}

void ParticleSystem::clear() { particles_.clear(); }

}  // namespace redgem
