#include "scene/particle_emitters.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace redgem {
namespace {

bool valid_rate(float rate) { return std::isfinite(rate) && rate > 0.0f && rate <= kMaxEmitterRate; }
bool time_passed(float dt) { return std::isfinite(dt) && dt > 0.0f; }
Vec3 world_position(const Transform& world, Vec3 offset) {
  const Vec3 scaled{offset.x * world.scale.x, offset.y * world.scale.y, offset.z * world.scale.z};
  return world.position + rotate(world.rotation, scaled);
}
// Keep the newest births when a frame asks for more than fits. Missed births never build a backlog.
int births_that_fit(double due, std::size_t flying) {
  const auto room = kMaxParticles - std::min(flying, kMaxParticles);
  return static_cast<int>(std::min(std::floor(due), static_cast<double>(room)));
}
float birth_fraction(int remaining, double fraction, float rate, float dt) {
  const double age = (static_cast<double>(remaining) + fraction) / rate;
  return static_cast<float>(std::clamp(1.0 - age / dt, 0.0, 1.0));
}

}  // namespace

void ParticleEmitters::discard_destroyed(const Scene& scene) {
  emitters_.erase(std::remove_if(emitters_.begin(), emitters_.end(),
                                 [&](const Emitter& emitter) { return scene.resolve(emitter.object) == kNoObject; }),
                  emitters_.end());
}

int ParticleEmitters::create(const Scene& scene, ObjectHandle object, const ParticleBurst& settings, float rate) {
  discard_destroyed(scene);
  if (!valid_rate(rate)) return kNoEmitter;
  if (emitters_.size() >= kMaxParticleEmitters) return kNoEmitter;
  if (last_id_ == std::numeric_limits<int>::max()) return kNoEmitter;
  const ObjectId resolved = scene.resolve(object);
  if (resolved == kNoObject) return kNoEmitter;
  const Vec3 position = world_position(scene.world_transform(resolved), settings.position);
  emitters_.push_back({++last_id_, object, settings, rate, 0.0, position});
  return last_id_;
}

bool ParticleEmitters::stop(int id) {
  const auto found = std::find_if(emitters_.begin(), emitters_.end(), [&](const Emitter& e) { return e.id == id; });
  if (found == emitters_.end()) return false;
  emitters_.erase(found);
  return true;
}

bool ParticleEmitters::active(const Scene& scene, int id) const {
  const auto found = std::find_if(emitters_.begin(), emitters_.end(), [&](const Emitter& e) { return e.id == id; });
  if (found == emitters_.end()) return false;
  return scene.resolve(found->object) != kNoObject;
}

void ParticleEmitters::emit_between(Emitter& emitter, float dt, const Transform& world, ParticleSystem& particles) {
  const double due = emitter.fraction + static_cast<double>(dt) * emitter.rate;
  emitter.fraction = std::fmod(due, 1.0);
  const Vec3 current = world_position(world, emitter.settings.position);
  ParticleBurst burst = emitter.settings;
  burst.count = 1;
  burst.direction = rotate(world.rotation, burst.direction);
  const int count = births_that_fit(due, particles.particles().size());
  for (int i = 0; i < count; ++i) {
    const float along = birth_fraction(count - 1 - i, emitter.fraction, emitter.rate, dt);
    burst.position = emitter.previous + (current - emitter.previous) * along;
    particles.burst(burst);
  }
}

void ParticleEmitters::advance_one(Emitter& emitter, float dt, const Scene& scene, ParticleSystem& particles) {
  const Transform world = scene.world_transform(scene.resolve(emitter.object));
  if (time_passed(dt)) emit_between(emitter, dt, world, particles);
  emitter.previous = world_position(world, emitter.settings.position);
}

void ParticleEmitters::advance(float dt, const Scene& scene, ParticleSystem& particles) {
  discard_destroyed(scene);
  for (Emitter& emitter : emitters_) advance_one(emitter, dt, scene, particles);
}

}  // namespace redgem
