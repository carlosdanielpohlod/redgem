// The objects the host's game placed, until their machine says they are there (placed_objects.hpp).

#include "net/placed_objects.hpp"

#include <iterator>

namespace redgem::net {

void PlacedObjects::place(int peer, const std::string& key, Vec3 position) {
  const Source source{peer, key};
  const bool room = placed_.size() < kMaxPlacedObjects || placed_.count(source) != 0;
  if (room) placed_[source] = Placed{position, 0.0f};
}

bool PlacedObjects::from_before(int peer, const std::string& key, Vec3 position) {
  const auto found = placed_.find({peer, key});
  if (found == placed_.end()) return false;
  if (length(position - found->second.position) > kPlacedReach) return true;
  placed_.erase(found);  // it heard: from here on its poses count
  return false;
}

void PlacedObjects::advance(float seconds) {
  for (auto placed = placed_.begin(); placed != placed_.end();) {
    placed->second.waited += seconds;
    placed = placed->second.waited > kPlacedWaitSeconds ? placed_.erase(placed) : std::next(placed);
  }
}

void PlacedObjects::forget(int peer) {
  placed_.erase(placed_.lower_bound({peer, std::string()}), placed_.lower_bound({peer + 1, std::string()}));
}

}  // namespace redgem::net
