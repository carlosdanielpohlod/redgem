#pragma once

#include <vector>

#include "scene/particles.hpp"
#include "scene/scene.hpp"

namespace redgem {

inline constexpr std::size_t kMaxParticleEmitters = 256;
inline constexpr float kMaxEmitterRate = 4096.0f;  // particles per game second
inline constexpr int kNoEmitter = 0;

// Emits in world space from an object's local offset and direction. Destroying the object ends
// emission; particles already in flight keep their own world positions and finish their lives.
class ParticleEmitters {
 public:
  int create(const Scene& scene, ObjectHandle object, const ParticleBurst& settings, float rate);
  bool stop(int id);
  bool active(const Scene& scene, int id) const;
  void advance(float dt, const Scene& scene, ParticleSystem& particles);
  void clear() { emitters_.clear(); }  // ids never repeat, including across scene changes

 private:
  struct Emitter {
    int id = kNoEmitter;
    ObjectHandle object;
    ParticleBurst settings;
    float rate = 0.0f;
    double fraction = 0.0;
    Vec3 previous;
  };
  void discard_destroyed(const Scene& scene);
  void advance_one(Emitter& emitter, float dt, const Scene& scene, ParticleSystem& particles);
  void emit_between(Emitter& emitter, float dt, const Transform& world, ParticleSystem& particles);

  std::vector<Emitter> emitters_;
  int last_id_ = kNoEmitter;
};

}  // namespace redgem
