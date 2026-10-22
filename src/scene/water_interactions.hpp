#pragma once

#include <vector>

#include "scene/water_geometry.hpp"
#include "scene/water_ripples.hpp"

namespace redgem {

inline constexpr std::size_t kMaxWaterBodies = 256;     // sampled moving bodies, in scene order
inline constexpr std::size_t kMaxWaterContacts = 1024;  // retained body/surface pairs
inline constexpr float kWaterContactSkin = 0.025f;      // meters of hysteresis around the undisplaced plane
inline constexpr float kWaterImpulseCooldown = 0.12f;   // simulation seconds between a pair's impulses
inline constexpr float kWaterWakeDistance = 0.25f;      // meters between wake packets
inline constexpr float kWaterMinimumSpeed = 0.15f;      // meters per second below which resting jitter is ignored
inline constexpr float kWaterImpulseSpeed = 8.0f;       // impact speed giving the maximum ripple strength

struct WaterBodySample {
  ObjectHandle object;
  Bounds bounds;  // world collider bounds, including rotation and scale
  Vec3 velocity;
};

class WaterInteractions {
 public:
  void reset(const SceneDesc& desc);
  bool enabled() const { return enabled_; }
  // Invalidates a script teleport's old sample: it is a new starting pose, not an impact sweep.
  void forget(ObjectHandle body);
  // A zero delta primes new bodies without aging or emitting. Only positive finite steps simulate.
  void step(const Scene& scene, const SceneDesc& desc, const std::vector<WaterBodySample>& bodies, float delta);
  const WaterRippleSnapshot& snapshot() const { return snapshot_; }

 private:
  struct Contact {
    ObjectHandle body;
    ObjectHandle surface;
    std::size_t primitive = 0;
    double emitted = -kWaterImpulseCooldown;
    Vec3 wake_origin;
    bool wet = false;
  };
  void age(float delta);
  void discard_dead(const Scene& scene);
  void interact(const WaterSurface& surface, const WaterBodySample& before, const WaterBodySample& body, float delta);
  void prune_contacts(const std::vector<WaterSurface>& surfaces, const std::vector<WaterBodySample>& bodies);
  bool active_contact(const Contact& contact, const std::vector<WaterSurface>& surfaces,
                      const std::vector<WaterBodySample>& bodies) const;
  void respond(Contact& contact, const WaterSurface& surface, const WaterBodySample& before,
               const WaterBodySample& body, const WaterSweep& sweep, float speed);
  void wake(Contact& contact, const WaterSurface& surface, const WaterBodySample& body, float speed);
  Contact* contact_for(const WaterSurface& surface, const WaterBodySample& before);
  void emit(Contact& contact, const WaterSurface& surface, const WaterBodySample& body, Vec3 point, float speed);
  const WaterBodySample* previous(ObjectHandle body) const;
  void keep_bodies(const Scene& scene, const std::vector<WaterBodySample>& bodies);
  void prime(const Scene& scene, const std::vector<WaterBodySample>& bodies);
  void interact_body(const Scene& scene, const std::vector<WaterSurface>& surfaces, const WaterBodySample& body,
                     float delta);
  void simulate(const Scene& scene, const SceneDesc& desc, const std::vector<WaterBodySample>& bodies, float delta);

  bool enabled_ = false;
  std::uint64_t next_serial_ = 1;
  WaterRippleSnapshot snapshot_;
  WaterGeometryCache geometry_;
  std::vector<WaterBodySample> previous_;
  std::vector<Contact> contacts_;
};

}  // namespace redgem
