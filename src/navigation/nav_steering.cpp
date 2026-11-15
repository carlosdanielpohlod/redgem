#include "navigation/nav_steering.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace redgem {
namespace {

constexpr float kNever = std::numeric_limits<float>::infinity();
constexpr float kFullTurn = kTwo * kPi;

Vec3 flat(Vec3 v) { return {v.x, 0.0f, v.z}; }

Vec3 no_faster(Vec3 v, float most) {
  const float speed = length(v);
  return speed > most ? v * (most / speed) : v;
}

// Whether going at `to` turns left of going at `from` (seen from above, y up).
bool turns_left(Vec3 from, Vec3 to) { return cross(from, to).y > 0.0f; }

Vec3 turned(Vec3 v, float radians) {
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  return {v.x * c - v.z * s, 0.0f, v.x * s + v.z * c};
}

// The direction the sampled velocities turn around: the wanted one, else the one it has, else any.
Vec3 heading_of(const SteeringAgent& agent) {
  const Vec3 wanted = normalize(flat(agent.preferred));
  if (length(wanted) > 0.0f) return wanted;
  const Vec3 moving = normalize(flat(agent.velocity));
  return length(moving) > 0.0f ? moving : Vec3::right();
}

// Whether two are at one place: nothing tells which way apart is, so neither is avoided.
bool on_top(Vec3 offset) { return length(offset) <= kEpsilon; }

bool standing(Vec3 velocity) { return length(velocity) <= kEpsilon; }

// Whether an obstacle is one to keep clear of: near enough to be met, and somewhere else.
bool worth_avoiding(const SteeringAgent& agent, const SteeringObstacle& other) {
  return !out_of_reach(agent, other) && !on_top(flat(other.position - agent.position));
}

class Steering {
 public:
  Steering(const SteeringAgent& agent, const std::vector<SteeringObstacle>& others, const VelocityCheck& allowed);
  Vec3 run();

 private:
  float hit_time(Vec3 candidate, const SteeringObstacle& other) const;
  float soonest_hit(Vec3 candidate) const;
  float penalty(Vec3 candidate) const;
  // Standing still always may be taken.
  bool takeable(Vec3 candidate) const { return standing(candidate) || !allowed_ || allowed_(candidate); }
  // Whether the wanted velocity is taken with nobody near: nothing else is worth trying.
  bool settled() const { return near_.empty() && best_penalty_ < kNever; }
  void consider(Vec3 candidate);
  void consider_ring(float speed);

  const SteeringAgent& agent_;
  std::vector<SteeringObstacle> near_;
  const VelocityCheck& allowed_;
  Vec3 preferred_;
  Vec3 best_;
  float best_penalty_ = kNever;
};

Steering::Steering(const SteeringAgent& agent, const std::vector<SteeringObstacle>& others,
                   const VelocityCheck& allowed)
    : agent_(agent), allowed_(allowed), preferred_(no_faster(flat(agent.preferred), agent.max_speed)) {
  for (const SteeringObstacle& other : others) {
    if (worth_avoiding(agent, other)) near_.push_back(other);
  }
}

float Steering::hit_time(Vec3 candidate, const SteeringObstacle& other) const {
  const Vec3 offset = flat(other.position - agent_.position);
  // Steering both, each takes half: the velocity it is taken to move at is its own one, pushed
  // twice as far from it.
  const Vec3 own = other.steers ? candidate * kTwo - flat(agent_.velocity) : candidate;
  const float reach = std::min(agent_.radius + other.radius, length(offset) * nav::kOverlapShare);
  return time_to_collision(offset, own - flat(other.velocity), reach);
}

float Steering::soonest_hit(Vec3 candidate) const {
  float soonest = kNever;
  for (const SteeringObstacle& other : near_) soonest = std::min(soonest, hit_time(candidate, other));
  return soonest;
}

float Steering::penalty(Vec3 candidate) const {
  const float hit = soonest_hit(candidate);
  const float danger = hit < nav::kSteeringHorizon ? nav::kAvoidanceWeight / hit : 0.0f;
  const float side = turns_left(preferred_, candidate) ? nav::kPassingSideBias : 0.0f;
  return length(candidate - preferred_) + danger + side;
}

void Steering::consider(Vec3 candidate) {
  if (!takeable(candidate)) return;
  const float score = penalty(candidate);
  if (score >= best_penalty_) return;
  best_penalty_ = score;
  best_ = candidate;
}

void Steering::consider_ring(float speed) {
  const Vec3 heading = heading_of(agent_) * speed;
  for (int step = 0; step < nav::kSteeringDirections; ++step) {
    consider(turned(heading, kFullTurn * static_cast<float>(step) / static_cast<float>(nav::kSteeringDirections)));
  }
}

Vec3 Steering::run() {
  consider(preferred_);
  if (settled()) return best_;
  for (int ring = nav::kSteeringSpeeds; ring > 0; --ring) {
    consider_ring(agent_.max_speed * static_cast<float>(ring) / static_cast<float>(nav::kSteeringSpeeds));
  }
  consider(Vec3::zero());
  return best_;
}

}  // namespace

float time_to_collision(Vec3 offset, Vec3 closing, float reach) {
  const float apart = dot(offset, offset) - reach * reach;
  if (apart <= 0.0f) return 0.0f;
  const float nearing = dot(offset, closing);
  if (nearing <= 0.0f) return kNever;
  const float speed = dot(closing, closing);
  const float discriminant = nearing * nearing - speed * apart;
  if (discriminant <= 0.0f) return kNever;
  return (nearing - std::sqrt(discriminant)) / speed;
}

bool out_of_reach(const SteeringAgent& agent, const SteeringObstacle& other) {
  const float gap = length(flat(other.position - agent.position)) - agent.radius - other.radius;
  return gap > nav::kSteeringHorizon * (agent.max_speed + length(flat(other.velocity)));
}

Vec3 steer(const SteeringAgent& agent, const std::vector<SteeringObstacle>& others, const VelocityCheck& allowed) {
  return Steering(agent, others, allowed).run();
}

}  // namespace redgem
