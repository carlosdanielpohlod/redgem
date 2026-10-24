// RelayServer: a poll's turn for every machine, and what keeps one machine from costing the
// others more than its share.

#include "net/rooms/relay_server.hpp"

#include <algorithm>

namespace redgem::net {

using namespace wire;

namespace {

// Whether a message for `to`, passed on in a room, goes to the machine that is `peer` there.
bool is_for(int to, int peer) { return to == kEveryone || to == peer; }

}  // namespace

bool RelayServer::start(std::uint16_t port, std::string& error) {
  listener_ = Socket::listen_on(port, error);
  if (!listener_.valid()) return false;
  port_ = listener_.local_port();
  return true;
}

void RelayServer::poll(float seconds) {
  if (!listener_.valid()) return;
  guessers_.forgive(seconds);
  accept_rest_ = std::max(0.0f, accept_rest_ - seconds);
  accept_newcomers();
  // By index: nothing below adds members, but entering a room touches other members.
  for (std::size_t i = 0; i < members_.size(); ++i) serve(*members_[i], seconds);
  shed_load();
  drop_closed_members();
}

bool RelayServer::wait(float seconds) const {
  // (A newcomer that cannot be taken would end every wait at once: it is not watched for a while.)
  const bool resting = accept_rest_ > 0.0f;
  std::vector<Watched> watched;
  if (!resting) watched.push_back({&listener_, false});
  for (const auto& member : members_) {
    // Messages that arrived and still wait their turn (see kMaxMessagesPerPoll) are work for now.
    if (member->connection.message_waiting()) return true;
    watched.push_back(member->connection.watched());
  }
  return Socket::wait(watched, resting ? std::min(seconds, accept_rest_) : seconds);
}

void RelayServer::serve(Member& member, float seconds) {
  Connection& connection = member.connection;
  if (!member.in_room()) wait_on(member, seconds);
  member.allowance.refill(seconds);
  charge(member, connection.pump());
  if (member.in_room()) connection.keep_in_touch(seconds);
  // (Only so many a poll: the rest wait for the next one, see kMaxMessagesPerPoll.)
  while (const auto message = connection.next_message()) {
    if (!charge(member, kRelayMessageCostBytes)) break;  // the message past what it may send goes nowhere
    handle(member, *message);
  }
}

void RelayServer::wait_on(Member& member, float seconds) {
  if (member.asking()) {
    await_verdict(member, seconds);
  } else {
    member.connection.keep_waiting(seconds);  // for it to say which room
  }
}

bool RelayServer::charge(Member& member, std::size_t bytes) {
  if (member.allowance.charge(bytes)) return true;
  member.connection.close();  // no game talks this much: a flood
  return false;
}

void RelayServer::shed_load() {
  std::size_t held = 0;
  for (const auto& member : members_) held += member->connection.held();
  // Whoever holds less than an even share is never the reason: with every machine under it the
  // total fits. The newest above it go first; those who were here before keep playing.
  const std::size_t share = max_buffered_bytes_ / kMaxRelayConnections;
  for (auto member = members_.rbegin(); member != members_.rend(); ++member) {
    if (held <= max_buffered_bytes_) return;
    const std::size_t holding = (*member)->connection.held();
    if (holding <= share) continue;
    (*member)->connection.close();
    held -= holding;
  }
}

void RelayServer::handle(Member& member, const nlohmann::json& message) {
  try {
    act_on(member, message);
  } catch (const nlohmann::json::exception&) {
    member.connection.close();  // (well_formed should have caught it: one machine never takes the relay down)
  }
}

void RelayServer::act_on(Member& member, const nlohmann::json& message) {
  if (member.asking()) return;  // nothing it says counts while the game of its room decides on it
  if (!member.in_room()) {
    hear_newcomer(member, message);
  } else if (is_game_message(message)) {
    pass_on(member, message);
  } else if (member.holds_room()) {
    hear_opener(member, message);
  }
}

void RelayServer::pass_on(Member& sender, const nlohmann::json& message) {
  // Who it is from is what the relay knows, not what the message claims.
  const nlohmann::json passed = passed_on(message, sender.peer);
  const int to = passed.value(kTo, kEveryone);
  const std::vector<std::uint8_t> bytes = packed(passed);  // once, however many it goes to
  for (Member* other : members_of(sender.room)) {
    if (other->peer == sender.peer) continue;
    if (is_for(to, other->peer)) other->connection.send_packed(bytes);
  }
}

void RelayServer::drop_closed_members() {
  // Telling a room that someone left can itself find another member gone (and that one may have
  // been the room's opener): go round until nothing more changes.
  for (bool changed = true; changed;) {
    close_rooms_without_opener();
    changed = tell_rooms_who_left();
  }
  for (const auto& member : members_) {
    if (member->connection.closed()) take_out_of_room(*member);
  }
  members_.erase(std::remove_if(members_.begin(), members_.end(),
                                [](const std::unique_ptr<Member>& member) { return member->connection.closed(); }),
                 members_.end());
}

}  // namespace redgem::net
