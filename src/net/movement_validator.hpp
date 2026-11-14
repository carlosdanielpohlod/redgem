#pragma once

#include <cstddef>
#include <limits>
#include <map>
#include <string>
#include <utility>

#include "core/math.hpp"

// Movement validation, an anti-cheat the game's host may turn on (Network.validate_movement): it
// checks where the machines that joined say their objects are (NetworkSync's poses) against how
// fast and how far an object of the game can move, and does not believe the moves past that.
//
// How fast is checked against a distance allowance, as ByteAllowance does for bytes: time earns an
// object the distance its top speed covers (and the tolerance on top), up to what kJitterSeconds
// earn, and every move spends it. Poses that arrive unevenly (two in one frame, then none for a
// while) spend what the time between them earned all the same, so the network's jitter is never a
// violation; a machine that keeps moving faster than allowed runs out. How far is checked per pose:
// a single move longer than the teleport distance is never believed.
namespace redgem::net {

// The share of the limits a move may go past and still be believed (0.25: a quarter over), unless
// the game says otherwise.
inline constexpr float kDefaultMovementTolerance = 0.25f;
// Seconds of movement at the top speed an object may make at once: what poses arriving unevenly
// (bunched together after a pause) add up to.
inline constexpr float kJitterSeconds = 0.5f;
// Seconds after a violation of an object during which its next ones are corrected but not counted:
// the machine's own poses that were already on their way before the correction reached it.
inline constexpr float kCorrectionSeconds = 1.0f;
// The objects of one machine whose movement is followed. Past that, the ones it has not moved for
// kStaleTrackSeconds are forgotten; when none are, its new objects' poses are not used at all (an
// object followed already is never forgotten for a new one: that would let a machine start its
// hero over anywhere by inventing objects).
inline constexpr std::size_t kMaxCheckedObjects = 4096;
inline constexpr double kStaleTrackSeconds = 10.0;

// What the host holds the moves of the others' objects to. A limit of 0 checks nothing of that kind.
struct MovementRules {
  float max_speed = 0.0f;          // meters a second an object may move
  float teleport_distance = 0.0f;  // meters one pose may move an object
  float tolerance = kDefaultMovementTolerance;
  int kick_after = 0;  // violations after which the machine is let go; 0: never
  bool undo = false;   // a move too fast is undone whole, rather than cut down to what was allowed

  // Whether anything is checked at all.
  bool enabled() const { return max_speed > 0.0f || teleport_distance > 0.0f; }
};

// What the host makes of a pose: whether it is believed, and where the object is then.
struct MovementVerdict {
  bool valid = true;
  Vec3 position;
  bool kick = false;     // the machine went past kick_after with this violation: it is let go
  bool ignored = false;  // a new object of a machine with as many followed as there may be: unused
};

class MovementValidator {
 public:
  // From now on, these rules (everything followed so far, and every count, starts over).
  void configure(const MovementRules& rules);
  const MovementRules& rules() const { return rules_; }
  bool enabled() const { return rules_.enabled(); }
  // Time passing on the host.
  void advance(float seconds) { clock_ += static_cast<double>(seconds); }
  // The host stood still for `seconds` (a level loading): what the others moved meanwhile, arriving
  // at once, is not held against them (up to `most_seconds` of it).
  void forgive(float seconds, float most_seconds);
  // Where machine `peer` says its object `key` is now: believed, or where it is believed to be.
  MovementVerdict check(int peer, const std::string& key, Vec3 position);
  // How many violations machine `peer` was caught at.
  int violations(int peer) const;
  // Machine `peer` left.
  void forget(int peer);
  // The object that goes by `key` is new on the host (its NetworkSync started, or it is gone): it
  // starts where its machine next says, whichever machine that is.
  void forget_object(const std::string& key);
  // Everything followed, and every count, starts over (the rules stay).
  void clear();

 private:
  struct Track {
    Vec3 accepted;                                              // where the object was last believed to be
    float budget = 0.0f;                                        // meters it may still move now
    float forgiven = 0.0f;                                      // ... and on top, what the host's stalls earned it
    double checked_at = 0.0;                                    // on the host's clock
    double counted_at = std::numeric_limits<double>::lowest();  // its last violation counted
  };
  using Source = std::pair<int, std::string>;  // a machine's id, and its object's key

  Track& track_of(const Source& source, Vec3 position);
  // Whether the object is followed, or there is room to follow it (once the stale ones go).
  bool can_track(const Source& source);
  bool tracked_or_room(const Source& source) const {
    return tracks_.count(source) != 0 || tracks_of(source.first) < kMaxCheckedObjects;
  }
  std::size_t tracks_of(int peer) const;
  void forget_tracks_of(int peer);
  void forget_stale_tracks_of(int peer);
  // Where the objects of `peer` start among those followed (the sources are in order of machine).
  std::map<Source, Track>::const_iterator first_track_of(int peer) const {
    return tracks_.lower_bound({peer, std::string()});
  }
  void refill(Track& track);
  // Whether a move of `moved` meters may be believed.
  bool allowed(const Track& track, float moved) const;
  bool teleports(float moved) const;
  bool too_fast(const Track& track, float moved) const;
  bool checks_speed() const { return rules_.max_speed > 0.0f; }
  void spend(Track& track, float moved);
  MovementVerdict accept(Track& track, Vec3 position, float moved);
  MovementVerdict refuse(int peer, Track& track, Vec3 position, float moved);
  // Where an object that moved too far is believed to be instead.
  Vec3 corrected(const Track& track, Vec3 position, float moved) const;
  // Counts a violation (none while the last one is still being corrected); true when it was.
  bool count_violation(int peer, Track& track);
  bool kicks(int peer) const;
  float allowed_speed() const { return rules_.max_speed * (1.0f + rules_.tolerance); }
  float burst() const { return allowed_speed() * kJitterSeconds; }

  MovementRules rules_;
  std::map<Source, Track> tracks_;
  std::map<int, int> violations_;
  // Seconds on the host, as precise after weeks of a dedicated server as at its start.
  double clock_ = 0.0;
};

}  // namespace redgem::net
