#include "physics/body_limit.hpp"

#include <algorithm>

#include <nlohmann/json.hpp>

#include "physics/physics_config.hpp"

namespace redgem {
namespace {

constexpr const char* kSection = "game.json physics: ";

bool within_body_range(std::int64_t asked) { return asked >= kFewestMaxBodies && asked <= kMostMaxBodies; }

bool usable_body_limit(const nlohmann::json& asked) {
  return asked.is_number_integer() && within_body_range(asked.get<std::int64_t>());
}

std::string body_limit_problem() {
  return std::string(kSection) + kMaxBodiesKey + " must be a whole number from " + std::to_string(kFewestMaxBodies) +
         " to " + std::to_string(kMostMaxBodies) + "; the world holds " + std::to_string(kDefaultMaxBodies) +
         " bodies";
}

}  // namespace

BodyCapacity body_capacity(std::uint32_t max_bodies) {
  const std::uint32_t bodies = std::clamp(max_bodies, kFewestMaxBodies, kMostMaxBodies);
  // The scratch memory a step takes grows with the bodies and contacts in it: as much per body as
  // at the default, never less than the default's.
  const std::size_t scale = std::max<std::size_t>(1, bodies / kDefaultMaxBodies);
  return {bodies, bodies, bodies, config::kPhysicsTempAllocatorBytes * scale};
}

BodyLimitReading body_limit_from_json(const nlohmann::json& physics) {
  BodyLimitReading reading;
  if (!physics.is_object() || !physics.contains(kMaxBodiesKey)) return reading;
  const nlohmann::json& asked = physics[kMaxBodiesKey];
  if (usable_body_limit(asked)) {
    reading.max_bodies = asked.get<std::uint32_t>();
  } else {
    reading.error = body_limit_problem();
  }
  return reading;
}

}  // namespace redgem
