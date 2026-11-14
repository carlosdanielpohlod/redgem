#pragma once

// Local steering for characters that walk among others (crowds). Each frame a character asks for
// the velocity nearest the one it wants (toward its next waypoint) that keeps clear of the others
// for the next few seconds. Velocities are sampled around the wanted one, and each is scored by how
// far it is from it plus how soon it would run into someone (a sampled velocity obstacle).
//
// Two characters that both steer share the avoidance: each takes half of it (reciprocal velocity
// obstacles), so they neither both swerve too far nor wobble. A moving body that does not steer (a
// rolling crate, the player) is avoided whole. Everything is on the ground: heights are ignored
// and the velocity given back is flat.

#include <functional>
#include <vector>

#include "core/math.hpp"

namespace redgem {

namespace nav {
// How far ahead a collision is avoided, in seconds: one further away costs nothing yet.
inline constexpr float kSteeringHorizon = 3.0f;
// The velocities tried: as many directions all around, at as many speeds up to the fastest.
inline constexpr int kSteeringDirections = 16;
inline constexpr int kSteeringSpeeds = 3;
// What a collision one second away costs, in meters per second of change from the wanted
// velocity (one twice as near costs twice as much).
inline constexpr float kAvoidanceWeight = 2.0f;
// What turning left costs, in meters per second: two characters face to face both keep right,
// instead of both stepping the same way.
inline constexpr float kPassingSideBias = 0.05f;
// How far ahead a velocity is checked to keep to walkable ground, in seconds.
inline constexpr float kSteeringLookahead = 0.5f;
// Two that overlap already are avoided as if their reach were this share of how far apart they
// are: moving apart costs nothing, coming nearer does.
inline constexpr float kOverlapShare = 0.9f;
}  // namespace nav

// The character that steers: where it is, how it moves now, how it wants to move, how wide it is
// and how fast it walks at most.
struct SteeringAgent {
  Vec3 position;
  Vec3 velocity;
  Vec3 preferred;
  float radius = 0.0f;
  float max_speed = 0.0f;
};

// Someone (or something) to keep clear of: `steers` when it is a character that avoids the agent
// too, and takes half the avoidance.
struct SteeringObstacle {
  Vec3 position;
  Vec3 velocity;
  float radius = 0.0f;
  bool steers = false;
};

// Whether a velocity may be taken (one that walks off the walkable ground may not). Standing
// still always may.
using VelocityCheck = std::function<bool(Vec3 velocity)>;

// When two discs on the ground touch: one at the origin and one at `offset`, coming nearer at
// `closing` (the first's velocity less the second's), touching at `reach` apart. 0 when they touch
// already, infinity when they never do.
float time_to_collision(Vec3 offset, Vec3 closing, float reach);

// Whether an obstacle is too far to be met within the horizon, as fast as both go.
bool out_of_reach(const SteeringAgent& agent, const SteeringObstacle& other);

// The velocity the agent walks at: flat, no faster than its fastest.
Vec3 steer(const SteeringAgent& agent, const std::vector<SteeringObstacle>& others, const VelocityCheck& allowed = {});

}  // namespace redgem
