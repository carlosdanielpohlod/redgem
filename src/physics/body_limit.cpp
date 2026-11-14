#include "physics/body_limit.hpp"

#include <algorithm>

#include <nlohmann/json.hpp>

#include "physics/collision_layers.hpp"
#include "physics/physics_config.hpp"

namespace redgem {
namespace {

bool says_body_limit(const nlohmann::json& physics) { return physics.is_object() && physics.contains(kMaxBodiesKey); }

bool within_body_range(std::int64_t asked) { return asked >= kFewestMaxBodies && asked <= kMostMaxBodies; }

bool usable_body_limit(const nlohmann::json& asked) {
  return asked.is_number_integer() && within_body_range(asked.get<std::int64_t>());
}

std::string body_limit_problem() {
  return std::string(kPhysicsSection) + kMaxBodiesKey + " must be a whole number from " +
         std::to_string(kFewestMaxBodies) + " to " + std::to_string(kMostMaxBodies) + "; the world holds " +
         std::to_string(kDefaultMaxBodies) + " bodies";
}

}  // namespace

BodyCapacity body_capacity(std::uint32_t max_bodies) {
  const std::uint32_t bodies = std::clamp(max_bodies, kFewestMaxBodies, kMostMaxBodies);
  // What a step handles grows with the bodies in it, never below the default's: the pairs and
  // contacts one for one, and the scratch memory by as much for every kDefaultMaxBodies begun.
  const std::uint32_t handled = std::max(bodies, kDefaultMaxBodies);
  const std::size_t scale = (std::size_t{handled} + kDefaultMaxBodies - 1) / kDefaultMaxBodies;
  return {bodies, handled, handled, config::kPhysicsTempAllocatorBytes * scale};
}

BodyLimitReading body_limit_from_json(const nlohmann::json& physics) {
  BodyLimitReading reading;
  if (!says_body_limit(physics)) return reading;
  const nlohmann::json& asked = physics[kMaxBodiesKey];
  if (usable_body_limit(asked)) {
    reading.max_bodies = asked.get<std::uint32_t>();
  } else {
    reading.error = body_limit_problem();
  }
  return reading;
}

}  // namespace redgem
