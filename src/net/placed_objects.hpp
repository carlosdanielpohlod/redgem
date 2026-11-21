#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <utility>

#include "core/math.hpp"

// The objects the host's game put somewhere (Network.teleport) whose machine has not yet said they
// are there. The poses it sent before it heard still arrive for a moment, from where the object
// was: the host uses none of them (nobody sees the object go back, and its machine is not put
// back either), until one from the placed spot says the machine heard.
namespace redgem::net {

inline constexpr float kPlacedReach = 1.5f;        // meters from the placed spot a pose counts as there
inline constexpr float kPlacedWaitSeconds = 2.0f;  // the most the host waits to hear one
inline constexpr std::size_t kMaxPlacedObjects = 4096;

class PlacedObjects {
 public:
  void place(int peer, const std::string& key, Vec3 position);
  // Whether a pose of machine `peer`'s object `key` at `position` is from before it heard of the
  // place: not to be used. One near the placed spot is not, and the object's poses count again.
  bool from_before(int peer, const std::string& key, Vec3 position);
  // Time passing on the host: past kPlacedWaitSeconds, an object's poses count again whatever they say.
  void advance(float seconds);
  // Machine `peer` left.
  void forget(int peer);
  bool empty() const { return placed_.empty(); }

 private:
  struct Placed {
    Vec3 position;
    float waited = 0.0f;
  };
  using Source = std::pair<int, std::string>;  // a machine's id, and its object's key

  std::map<Source, Placed> placed_;
};

}  // namespace redgem::net
