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
float birth_age(int remaining, double fraction, float rate, float dt) {
  const double age = (static_cast<double>(remaining) + fraction) / rate;
  return static_cast<float>(std::clamp(age, 0.0, static_cast<double>(dt)));
}
Vec3 trail_start(Vec3 previous, Vec3 current) {
  const bool continuous = length(current - previous) <= kMaxEmitterTrailDistance;
  return continuous ? previous : current;
}
ParticleBurst world_burst(const ParticleBurst& settings, const Transform& world) {
  ParticleBurst burst = settings;
  burst.count = 1;
  burst.position = world_position(world, settings.position);
  burst.direction = rotate(world.rotation, settings.direction);
  return burst;
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

int ParticleEmitters::emit_births(const Emitter& emitter, const ParticleBurst& burst,
                                  const ParticleSystem::PreparedBurst& prepared, float dt, int count,
                                  ParticleSystem& particles) {
  const Vec3 from = trail_start(emitter.previous, burst.position);
  int added = 0;
  for (int i = 0; i < count; ++i) {
    const float age = birth_age(count - 1 - i, emitter.fraction, emitter.rate, dt);
    const Vec3 position = from + (burst.position - from) * (1.0f - age / dt);
    if (particles.emit_prepared(burst, prepared, position, age)) ++added;
  }
  return added;
}

int ParticleEmitters::emit_between(Emitter& emitter, float dt, const Transform& world, ParticleSystem& particles) {
  const double due = emitter.fraction + static_cast<double>(dt) * emitter.rate;
  emitter.fraction = std::fmod(due, 1.0);
  const int count = births_that_fit(due, particles.particles().size());
  if (count == 0) return 0;
  const ParticleBurst burst = world_burst(emitter.settings, world);
  const auto prepared = particles.prepare(burst);
  return emit_births(emitter, burst, prepared, dt, count, particles);
}

int ParticleEmitters::advance_one(Emitter& emitter, float dt, const Scene& scene, ParticleSystem& particles) {
  const Transform world = scene.world_transform(scene.resolve(emitter.object));
  const int added = time_passed(dt) ? emit_between(emitter, dt, world, particles) : 0;
  emitter.previous = world_position(world, emitter.settings.position);
  return added;
}

void ParticleEmitters::advance(float dt, const Scene& scene, ParticleSystem& particles) {
  discard_destroyed(scene);
  if (emitters_.empty()) return;
  const std::size_t start = next_emitter_ % emitters_.size();
  for (std::size_t i = 0; i < emitters_.size(); ++i) {
    const std::size_t index = (start + i) % emitters_.size();
    if (advance_one(emitters_[index], dt, scene, particles) > 0) next_emitter_ = (index + 1) % emitters_.size();
  }
}

}  // namespace redgem
