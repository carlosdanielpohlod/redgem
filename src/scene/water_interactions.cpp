#include "scene/water_interactions.hpp"

#include <algorithm>
#include <cmath>

namespace redgem {
namespace {

constexpr float kTeleportDistance = 20.0f;  // unreported jumps larger than this need velocity evidence
constexpr float kMotionRoom = 2.0f;         // allow acceleration and numerical integration differences
constexpr float kMinimumStep = 1e-5f;       // seconds: bound speed estimates for arbitrarily small input steps
constexpr float kMinimumStrength = 0.01f;   // meters: small valid contacts remain visible

bool is_water(const MaterialDesc& material) { return material.water.enabled; }

bool ordered_bounds(const Bounds& bounds) {
  return bounds.min.x <= bounds.max.x && bounds.min.y <= bounds.max.y && bounds.min.z <= bounds.max.z;
}

bool valid_body(const Scene& scene, const WaterBodySample& body) {
  if (scene.resolve(body.object) == kNoObject) return false;
  return body.bounds.valid && ordered_bounds(body.bounds) && finite_water_point(body.bounds.min) &&
         finite_water_point(body.bounds.max) && finite_water_point(body.velocity);
}

bool touches_plane(const WaterSurface& surface, const Bounds& bounds) {
  const float plane = surface.bounds.center().y;
  return bounds.min.y <= plane + kWaterContactSkin && bounds.max.y >= plane - kWaterContactSkin;
}

bool below_plane(bool previous, float bottom, float plane) {
  if (bottom > plane + kWaterContactSkin) return false;
  if (bottom < plane - kWaterContactSkin) return true;
  return previous;
}

float horizontal_distance(Vec3 from, Vec3 to) { return length(Vec3{to.x - from.x, 0.0f, to.z - from.z}); }

float motion_speed(const WaterBodySample& before, const WaterBodySample& body, float delta) {
  return std::max(length(body.velocity),
                  length(body.bounds.center() - before.bounds.center()) / std::max(delta, kMinimumStep));
}

bool discontinuous(const WaterBodySample& before, const WaterBodySample& body, float delta) {
  const float distance = length(body.bounds.center() - before.bounds.center());
  const float allowed = std::max(kTeleportDistance, length(body.velocity) * delta * kMotionRoom);
  return distance > allowed;
}

bool same_contact(const WaterSurface& surface, ObjectHandle owner, ObjectHandle target, std::size_t primitive,
                  ObjectHandle body) {
  return owner == body && target == surface.object && primitive == surface.primitive;
}

bool boundary_changed(const WaterSurface& surface, const Bounds& before, const Bounds& current, bool changed) {
  if (changed) return true;
  const auto old = water_contact_point(surface, before);
  if (!old) return true;
  if (!touches_plane(surface, before)) return true;
  return !water_contact_point(surface, current);
}

Bounds contact_margin(Bounds bounds) {
  bounds.min.y -= kWaterContactSkin;
  bounds.max.y += kWaterContactSkin;
  return bounds;
}

bool retains_contact(const WaterSurface& surface, const Bounds& before, const Bounds& current) {
  return water_swept_contact(surface, contact_margin(before), contact_margin(current)).has_value();
}

bool exiting_boundary(const WaterSurface& surface, const Bounds& before, const Bounds& current, bool rising_exit) {
  if (rising_exit) return true;
  if (!touches_plane(surface, before)) return false;
  if (!water_contact_point(surface, before)) return false;
  return !water_contact_point(surface, current);
}

Vec3 boundary_origin(const WaterSurface& surface, const Bounds& before, const Bounds& current, const WaterSweep& sweep,
                     bool rising_exit) {
  if (!exiting_boundary(surface, before, current, rising_exit)) return sweep.point;
  // Reversing the sweep finds the last point where this body touched the surface as it left.
  const auto exit = water_swept_contact(surface, current, before);
  return exit ? exit->point : sweep.point;
}

float ripple_radius(const Bounds& body) {
  const Vec3 half = body.half_extents();
  return std::clamp(std::max(half.x, half.z), kWaterRippleMinRadius, kWaterRippleMaxRadius);
}

}  // namespace

void WaterInteractions::reset(const SceneDesc& desc) {
  enabled_ = std::any_of(desc.materials.begin(), desc.materials.end(), is_water);
  snapshot_ = WaterRippleSnapshot{};
  next_serial_ = 1;
  previous_.clear();
  contacts_.clear();
  geometry_.clear();
}

void WaterInteractions::forget(ObjectHandle body) {
  previous_.erase(std::remove_if(previous_.begin(), previous_.end(),
                                 [body](const WaterBodySample& sample) { return sample.object == body; }),
                  previous_.end());
  contacts_.erase(std::remove_if(contacts_.begin(), contacts_.end(),
                                 [body](const Contact& contact) { return contact.body == body; }),
                  contacts_.end());
}

void WaterInteractions::age(float delta) {
  snapshot_.time += static_cast<double>(delta);
  std::size_t kept = 0;
  for (std::size_t i = 0; i < snapshot_.count; ++i) {
    WaterRipple ripple = snapshot_.ripples[i];
    ripple.age += delta;
    if (ripple.age < kWaterRippleLifetime) snapshot_.ripples[kept++] = ripple;
  }
  snapshot_.count = kept;
}

void WaterInteractions::discard_dead(const Scene& scene) {
  contacts_.erase(std::remove_if(contacts_.begin(), contacts_.end(),
                                 [&scene](const Contact& contact) {
                                   return scene.resolve(contact.body) == kNoObject ||
                                          scene.resolve(contact.surface) == kNoObject;
                                 }),
                  contacts_.end());
  std::size_t kept = 0;
  for (std::size_t i = 0; i < snapshot_.count; ++i) {
    const auto& ripple = snapshot_.ripples[i];
    if (scene.resolve(ripple.surface) != kNoObject) snapshot_.ripples[kept++] = ripple;
  }
  snapshot_.count = kept;
}

const WaterBodySample* WaterInteractions::previous(ObjectHandle body) const {
  const auto found = std::find_if(previous_.begin(), previous_.end(),
                                  [body](const WaterBodySample& sample) { return sample.object == body; });
  return found != previous_.end() ? &*found : nullptr;
}

void WaterInteractions::keep_bodies(const Scene& scene, const std::vector<WaterBodySample>& bodies) {
  previous_.clear();
  prime(scene, bodies);
}

void WaterInteractions::prime(const Scene& scene, const std::vector<WaterBodySample>& bodies) {
  for (const auto& body : bodies) {
    if (previous_.size() >= kMaxWaterBodies) break;
    if (valid_body(scene, body) && previous(body.object) == nullptr) previous_.push_back(body);
  }
}

WaterInteractions::Contact* WaterInteractions::contact_for(const WaterSurface& surface, const WaterBodySample& before) {
  for (Contact& contact : contacts_) {
    if (same_contact(surface, contact.body, contact.surface, contact.primitive, before.object)) return &contact;
  }
  if (contacts_.size() >= kMaxWaterContacts) return nullptr;
  Contact contact;
  contact.body = before.object;
  contact.surface = surface.object;
  contact.primitive = surface.primitive;
  contact.wet = before.bounds.min.y <= surface.bounds.center().y;
  contact.wake_origin = before.bounds.center();
  contacts_.push_back(contact);
  return &contacts_.back();
}

void WaterInteractions::emit(Contact& contact, const WaterSurface& surface, const WaterBodySample& body, Vec3 point,
                             float speed) {
  if (snapshot_.time - contact.emitted < kWaterImpulseCooldown) return;
  const float strength =
      std::clamp(speed / kWaterImpulseSpeed * kWaterRippleMaxStrength, kMinimumStrength, kWaterRippleMaxStrength);
  WaterRipple ripple{next_serial_++, surface.object, surface.primitive,         point,
                     0.0f,           strength,       ripple_radius(body.bounds)};
  if (snapshot_.count == kMaxWaterRipples) {
    std::move(snapshot_.ripples.begin() + 1, snapshot_.ripples.end(), snapshot_.ripples.begin());
    --snapshot_.count;
  }
  snapshot_.ripples[snapshot_.count++] = ripple;
  contact.emitted = snapshot_.time;
  contact.wake_origin = body.bounds.center();
}

void WaterInteractions::wake(Contact& contact, const WaterSurface& surface, const WaterBodySample& body, float speed) {
  if (horizontal_distance(contact.wake_origin, body.bounds.center()) < kWaterWakeDistance) return;
  const auto point = water_contact_point(surface, body.bounds);
  if (point) emit(contact, surface, body, *point, speed);
}

void WaterInteractions::respond(Contact& contact, const WaterSurface& surface, const WaterBodySample& before,
                                const WaterBodySample& body, const WaterSweep& sweep, float speed) {
  const bool wet = below_plane(contact.wet, body.bounds.min.y, surface.bounds.center().y);
  const bool changed = wet != contact.wet;
  const bool rising_exit = contact.wet && !wet;
  contact.wet = wet;
  if (speed < kWaterMinimumSpeed) return;
  if (boundary_changed(surface, before.bounds, body.bounds, changed))
    emit(contact, surface, body, boundary_origin(surface, before.bounds, body.bounds, sweep, rising_exit), speed);
  else
    wake(contact, surface, body, speed);
}

void WaterInteractions::interact(const WaterSurface& surface, const WaterBodySample& before,
                                 const WaterBodySample& body, float delta) {
  const auto sweep = water_swept_contact(surface, before.bounds, body.bounds);
  if (!sweep) return;
  Contact* contact = contact_for(surface, before);
  if (contact != nullptr) respond(*contact, surface, before, body, *sweep, motion_speed(before, body, delta));
}

bool WaterInteractions::active_contact(const Contact& contact, const std::vector<WaterSurface>& surfaces,
                                       const std::vector<WaterBodySample>& bodies) const {
  const auto* before = previous(contact.body);
  if (before == nullptr) return false;
  const auto body =
      std::find_if(bodies.begin(), bodies.end(), [&contact](const auto& b) { return b.object == contact.body; });
  if (body == bodies.end()) return false;
  for (const auto& surface : surfaces) {
    if (same_contact(surface, contact.body, contact.surface, contact.primitive, body->object))
      return retains_contact(surface, before->bounds, body->bounds);
  }
  return false;
}

void WaterInteractions::prune_contacts(const std::vector<WaterSurface>& surfaces,
                                       const std::vector<WaterBodySample>& bodies) {
  contacts_.erase(std::remove_if(contacts_.begin(), contacts_.end(),
                                 [this, &surfaces, &bodies](const auto& contact) {
                                   return !active_contact(contact, surfaces, bodies);
                                 }),
                  contacts_.end());
}

void WaterInteractions::interact_body(const Scene& scene, const std::vector<WaterSurface>& surfaces,
                                      const WaterBodySample& body, float delta) {
  if (!valid_body(scene, body)) return;
  const auto* before = previous(body.object);
  if (before == nullptr || discontinuous(*before, body, delta)) return;
  for (const WaterSurface& surface : surfaces) interact(surface, *before, body, delta);
}

void WaterInteractions::simulate(const Scene& scene, const SceneDesc& desc, const std::vector<WaterBodySample>& bodies,
                                 float delta) {
  age(delta);
  const auto& surfaces = geometry_.refresh(scene, desc);
  prune_contacts(surfaces, bodies);
  const std::size_t count = std::min(bodies.size(), kMaxWaterBodies);
  for (std::size_t i = 0; i < count; ++i) interact_body(scene, surfaces, bodies[i], delta);
  keep_bodies(scene, bodies);
}

void WaterInteractions::step(const Scene& scene, const SceneDesc& desc, const std::vector<WaterBodySample>& bodies,
                             float delta) {
  if (!enabled_) return;
  if (!std::isfinite(delta) || delta < 0.0f) return;
  discard_dead(scene);
  if (delta == 0.0f)
    prime(scene, bodies);
  else
    simulate(scene, desc, bodies, delta);
}

}  // namespace redgem
