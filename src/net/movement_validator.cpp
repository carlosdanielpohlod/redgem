// Movement validation: how fast and how far the others' objects may move, as the host believes it.

#include "net/movement_validator.hpp"

#include <algorithm>
#include <iterator>

namespace redgem::net {

void MovementValidator::configure(const MovementRules& rules) {
  rules_ = rules;
  clear();
}

void MovementValidator::clear() {
  tracks_.clear();
  violations_.clear();
}

void MovementValidator::forgive(float seconds, float most_seconds) {
  const float most = most_seconds * allowed_speed();
  const float earned = seconds * allowed_speed();
  for (auto& entry : tracks_) entry.second.forgiven = std::min(entry.second.forgiven + earned, most);
}

MovementVerdict MovementValidator::check(int peer, const std::string& key, Vec3 position) {
  Track& track = track_of({peer, key}, position);
  refill(track);
  const float moved = length(position - track.accepted);
  if (allowed(track, moved)) return accept(track, position, moved);
  return refuse(peer, track, position, moved);
}

int MovementValidator::violations(int peer) const {
  const auto found = violations_.find(peer);
  return found != violations_.end() ? found->second : 0;
}

void MovementValidator::forget(int peer) {
  forget_tracks_of(peer);
  violations_.erase(peer);
}

MovementValidator::Track& MovementValidator::track_of(const Source& source, Vec3 position) {
  const auto found = tracks_.find(source);
  if (found != tracks_.end()) return found->second;
  if (tracks_of(source.first) >= kMaxCheckedObjects) forget_tracks_of(source.first);
  // An object heard of for the first time is where its machine says: there is nothing to check it against.
  Track fresh;
  fresh.accepted = position;
  fresh.budget = burst();
  fresh.checked_at = clock_;
  return tracks_.emplace(source, fresh).first->second;
}

std::size_t MovementValidator::tracks_of(int peer) const {
  return static_cast<std::size_t>(std::distance(first_track_of(peer), first_track_of(peer + 1)));
}

void MovementValidator::forget_tracks_of(int peer) { tracks_.erase(first_track_of(peer), first_track_of(peer + 1)); }

void MovementValidator::refill(Track& track) {
  track.budget = std::min(burst(), track.budget + (clock_ - track.checked_at) * allowed_speed());
  track.checked_at = clock_;
}

bool MovementValidator::allowed(const Track& track, float moved) const {
  return !teleports(moved) && !too_fast(track, moved);
}

bool MovementValidator::teleports(float moved) const {
  return rules_.teleport_distance > 0.0f && moved > rules_.teleport_distance * (1.0f + rules_.tolerance);
}

bool MovementValidator::too_fast(const Track& track, float moved) const {
  return checks_speed() && moved > track.budget + track.forgiven;
}

void MovementValidator::spend(Track& track, float moved) {
  if (!checks_speed()) return;
  track.budget -= moved;
  if (track.budget >= 0.0f) return;
  track.forgiven = std::max(0.0f, track.forgiven + track.budget);  // what went past it, out of what was forgiven
  track.budget = 0.0f;
}

MovementVerdict MovementValidator::accept(Track& track, Vec3 position, float moved) {
  spend(track, moved);
  track.accepted = position;
  return {true, position, false};
}

MovementVerdict MovementValidator::refuse(int peer, Track& track, Vec3 position, float moved) {
  const Vec3 instead = corrected(track, position, moved);
  spend(track, length(instead - track.accepted));
  track.accepted = instead;
  const bool counted = count_violation(peer, track);
  return {false, instead, counted && kicks(peer)};
}

Vec3 MovementValidator::corrected(const Track& track, Vec3 position, float moved) const {
  // A teleport is never believed in part; a move too fast goes as far as was allowed, unless undone.
  if (rules_.undo || teleports(moved)) return track.accepted;
  const float allowed_distance = track.budget + track.forgiven;
  return track.accepted + (position - track.accepted) * (allowed_distance / moved);
}

bool MovementValidator::count_violation(int peer, Track& track) {
  if (clock_ - track.counted_at < kCorrectionSeconds) return false;
  track.counted_at = clock_;
  ++violations_[peer];
  return true;
}

bool MovementValidator::kicks(int peer) const { return rules_.kick_after > 0 && violations(peer) >= rules_.kick_after; }

}  // namespace redgem::net
