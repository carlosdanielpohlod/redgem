#pragma once

// How many bodies a scene's physics world holds at once, as game.json's "physics" section says:
//   "physics": {"max_bodies": 16384}
// The world is sized for it when the scene loads: past it, an object gets no body (said once).
// The body pairs and contacts a step can handle, and the step's scratch memory, grow with it,
// never below what they are at the default.

#include <cstddef>
#include <cstdint>
#include <string>

#include <nlohmann/json_fwd.hpp>

namespace redgem {

inline constexpr const char* kMaxBodiesKey = "max_bodies";
inline constexpr std::uint32_t kDefaultMaxBodies = 4096;
// Fewer would leave no room for a level's walls and floors besides what moves on them.
inline constexpr std::uint32_t kFewestMaxBodies = 64;
// Far below what Jolt can index (about 8 million): the scratch memory of a step reserved for this
// many is 160 MB, as much as a game should spend on it.
inline constexpr std::uint32_t kMostMaxBodies = 65536;

// What the world is sized for, given how many bodies it holds.
struct BodyCapacity {
  std::uint32_t bodies = kDefaultMaxBodies;
  std::uint32_t body_pairs = kDefaultMaxBodies;
  std::uint32_t contact_constraints = kDefaultMaxBodies;
  std::size_t scratch_bytes = 0;
};
BodyCapacity body_capacity(std::uint32_t max_bodies);

// The "max_bodies" of game.json's "physics" section: the default when the section has none, and
// also, with what is wrong, when it is no whole number from kFewestMaxBodies to kMostMaxBodies.
struct BodyLimitReading {
  std::uint32_t max_bodies = kDefaultMaxBodies;
  std::string error;
};
BodyLimitReading body_limit_from_json(const nlohmann::json& physics);

}  // namespace redgem
