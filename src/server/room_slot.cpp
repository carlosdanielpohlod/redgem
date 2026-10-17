#include "server/room_slot.hpp"

#include <algorithm>

namespace redgem::server {

using Hold = std::unique_lock<std::mutex>;

bool RoomSlot::hand(net::Arrival& arrival) {
  const Hold hold(lock_);
  if (closed_) return false;
  arrivals_.push_back(std::move(arrival));
  return true;
}

std::size_t RoomSlot::places_taken() const {
  const Hold hold(lock_);
  return places_taken_locked();
}

std::size_t RoomSlot::places_taken_locked() const { return players_ + deciding_ + arrivals_.size(); }

std::size_t RoomSlot::players() const {
  const Hold hold(lock_);
  return players_;
}

int RoomSlot::links_from(const std::string& address) const {
  const Hold hold(lock_);
  const auto on_the_way = std::count_if(arrivals_.begin(), arrivals_.end(), [&address](const net::Arrival& arrival) {
    return arrival.connection.address() == address;
  });
  return static_cast<int>(std::count(addresses_.begin(), addresses_.end(), address) + on_the_way);
}

bool RoomSlot::closed() const {
  const Hold hold(lock_);
  return closed_;
}

bool RoomSlot::visit(const std::function<void(Engine&)>& visitor) {
  const auto visit = std::make_shared<Visit>(Visit{visitor});
  Hold hold(lock_);
  if (closed_) return false;
  visits_.push_back(visit);
  visited_.wait(hold, [&visit] { return visit->done; });
  return visit->ran;
}

void RoomSlot::admit_arrivals(net::NetworkSession& network) {
  // (All under the lock: the lobby never sees a newcomer in neither place.)
  const Hold hold(lock_);
  for (net::Arrival& arrival : arrivals_) network.admit_arrival(std::move(arrival.connection), arrival.auth);
  arrivals_.clear();
  census_locked(network);
}

void RoomSlot::run_visits(Engine& game) {
  Visits visits;
  {
    const Hold hold(lock_);
    visits.swap(visits_);
  }
  try {
    for (const auto& visit : visits) visit->visitor(game);
  } catch (...) {
    finish(visits, false);  // nobody is left waiting on a room that broke
    throw;
  }
  finish(visits, true);
}

void RoomSlot::take_census(const net::NetworkSession& network) {
  const Hold hold(lock_);
  census_locked(network);
}

bool RoomSlot::close_when_over(bool game_quit) {
  const Hold hold(lock_);
  if (!match_over_locked(game_quit)) return false;
  close_locked();
  return true;
}

void RoomSlot::close() {
  const Hold hold(lock_);
  close_locked();
}

bool RoomSlot::match_over_locked(bool game_quit) const { return game_quit || places_taken_locked() == 0; }

void RoomSlot::census_locked(const net::NetworkSession& network) {
  players_ = network.peers().size();
  deciding_ = network.deciding();
  addresses_ = network.addresses();
}

void RoomSlot::close_locked() {
  closed_ = true;
  arrivals_.clear();  // (their connections close with them: the machines hear the game is gone)
  players_ = 0;
  deciding_ = 0;
  addresses_.clear();
  for (const auto& visit : visits_) visit->done = true;
  visits_.clear();
  visited_.notify_all();
}

void RoomSlot::finish(const Visits& visits, bool ran) {
  const Hold hold(lock_);
  for (const auto& visit : visits) {
    visit->ran = ran;
    visit->done = true;
  }
  visited_.notify_all();
}

}  // namespace redgem::server
