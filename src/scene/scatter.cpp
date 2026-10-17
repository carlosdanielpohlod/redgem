#include "scene/scatter.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <utility>

#include "scene/dice.hpp"

namespace redgem {
namespace {

constexpr float kFullTurn = kTwo * kPi;  // radians
constexpr float kLevel = 1e-6f;          // how little a ground may lean and still count as level

// The turn that leans the vertical over to `normal` (none on level ground).
Quat lean_to(Vec3 normal) {
  const Vec3 axis = cross(Vec3::up(), normal);
  const float sine = length(axis);
  if (sine <= kLevel) return Quat::identity();
  return quat_from_axis_angle(axis / sine, std::atan2(sine, dot(Vec3::up(), normal)));
}

// What is under a point of the box: the ground, or the box's bottom when there is no ground.
std::optional<GroundHit> under(const ScatterRequest& request, const GroundProbe& ground, float x, float z) {
  if (!ground) return GroundHit{{x, request.low.y, z}, Vec3::up()};
  return ground({x, request.high.y, z}, request.high.y - request.low.y);
}

std::optional<Transform> place_one(Dice& dice, const ScatterRequest& request, const GroundProbe& ground) {
  const float x = dice.between(request.low.x, request.high.x);
  const float z = dice.between(request.low.z, request.high.z);
  const Quat turn = quat_from_axis_angle(Vec3::up(), dice.between(0.0f, kFullTurn));
  const float scale = dice.between(request.smallest, request.largest);
  const std::optional<GroundHit> hit = under(request, ground, x, z);
  if (!hit) return std::nullopt;
  Transform place;
  place.position = hit->point;
  place.rotation = request.align ? lean_to(hit->normal) * turn : turn;
  place.scale = Vec3::one() * scale;
  return place;
}

}  // namespace

std::vector<Transform> scatter_places(const ScatterRequest& request, const GroundProbe& ground) {
  Dice dice(request.seed);
  std::vector<Transform> places;
  places.reserve(request.count);
  for (std::size_t i = 0; i < request.count; ++i) {
    if (const std::optional<Transform> place = place_one(dice, request, ground)) places.push_back(*place);
  }
  return places;
}

void scale_places(std::vector<Transform>& places, Vec3 scale) {
  for (Transform& place : places) place.scale = hadamard(place.scale, scale);
}

// --- A scene's sets -----------------------------------------------------------------------------

int Scattered::add(ScatterSet set) {
  if (copies() + set.places.size() > kMaxScatteredCopies) return 0;
  set.id = next_id_++;
  sets_.push_back(std::move(set));
  changed();
  return sets_.back().id;
}

bool Scattered::remove(int id) {
  const auto found = std::find_if(sets_.begin(), sets_.end(), [id](const ScatterSet& set) { return set.id == id; });
  if (found == sets_.end()) return false;
  sets_.erase(found);
  changed();
  return true;
}

void Scattered::clear() {
  sets_.clear();
  changed();
}

std::size_t Scattered::copies() const {
  std::size_t total = 0;
  for (const ScatterSet& set : sets_) total += set.places.size();
  return total;
}

void Scattered::changed() {
  // For the whole process, not one scene (see version()).
  static std::atomic<std::uint64_t> last{0};
  version_ = ++last;
}

}  // namespace redgem
