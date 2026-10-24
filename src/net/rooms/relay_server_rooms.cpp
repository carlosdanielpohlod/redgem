// RelayServer: its rooms. Whoever asks for a new one opens it and gets its code, the others
// enter with the code, and a room ends with the machine that opened it.

#include "net/rooms/relay_server.hpp"

#include <algorithm>

namespace redgem::net {

using namespace wire;

namespace {

// A newcomer's first words say which room it wants.
bool asks_for_a_room(const nlohmann::json& message) { return message[kKind] == kHello && message.contains(kRoom); }

// What a newcomer let into a room as `id` is told: its id, who else is there, and the room's code.
nlohmann::json welcome(int id, const std::vector<int>& others, const std::string& room) {
  return {{kKind, kWelcome}, {kTo, id}, {kPeers, others}, {kRoom, room}, {kVersion, kProtocolVersion}};
}

}  // namespace

std::string RelayServer::new_room_code() const {
  return unused_room_code([this](const std::string& code) { return rooms_.count(code) != 0; });
}

void RelayServer::hear_newcomer(Member& member, const nlohmann::json& message) {
  // Anything else than which room, and it is not let in.
  if (!asks_for_a_room(message)) {
    member.connection.close();
    return;
  }
  // (Whether the game of a room it opens decides who comes in: from the start, before anyone joins.)
  member.guarded = message.value(kGuards, false);
  enter_room(member, message[kRoom], message.value(kAuth, nlohmann::json()));
}

void RelayServer::enter_room(Member& member, const std::string& wanted, const nlohmann::json& auth) {
  if (wanted == kNewRoom) {
    open_room(member);
  } else if (rooms_.count(wanted) == 0) {
    note_wrong_code(member);
  } else {
    join_room(member, wanted, auth);
  }
}

int RelayServer::rooms_opened_by(const std::string& address) const {
  return static_cast<int>(std::count_if(members_.begin(), members_.end(), [&address](const auto& member) {
    return member->holds_room() && member->connected_from(address);
  }));
}

void RelayServer::open_room(Member& member) {
  const bool room_to_give =
      rooms_.size() < kMaxRelayRooms && rooms_opened_by(member.connection.address()) < limits_.rooms_per_address;
  const std::string code = room_to_give ? new_room_code() : std::string();
  if (code.empty()) {
    member.connection.close();
    return;
  }
  place_in(member, code);
  member.peer = kHostId;
  member.connection.send(welcome(member.peer, {}, code));
}

void RelayServer::join_room(Member& member, const std::string& room, const nlohmann::json& auth) {
  const int id = free_peer_id(seats_taken(room));
  Member* opener = opener_of(room);
  if (!takes_newcomers(id, opener)) {
    member.connection.close();
    return;
  }
  place_in(member, room);
  if (opener->guarded) {
    ask(member, *opener, id, auth);
  } else {
    let_in(member, id);
  }
}

bool RelayServer::takes_newcomers(int free_id, const Member* opener) { return free_id != 0 && opener != nullptr; }

void RelayServer::let_in(Member& member, int id) {
  std::vector<int> ids;
  for (const Member* other : members_of(member.room)) ids.push_back(other->peer);
  member.peer = id;
  member.asking_as = 0;
  member.connection.send(welcome(id, ids, member.room));
  tell_room(member.room, id, {{kKind, kArrived}, {kFrom, id}});
}

bool RelayServer::has_opener(const std::vector<Member*>& inside) {
  return std::any_of(inside.begin(), inside.end(), [](const Member* member) { return member->holds_room(); });
}

void RelayServer::close_rooms_without_opener() {
  // A room ends with the machine that opened it: everyone in it is let go.
  for (auto room = rooms_.begin(); room != rooms_.end();) {
    const std::vector<Member*> inside = members_of(room->first);
    if (has_opener(inside)) {
      ++room;
      continue;
    }
    empty_room(room->first);
    room = rooms_.erase(room);
  }
}

void RelayServer::empty_room(const std::string& room) {
  for (const auto& member : members_) {
    if (member->holds_seat_in(room)) member->connection.close();
  }
}

bool RelayServer::missed_by_a_room(const Member& member) const {
  return member.seat() >= kFirstGuestId && rooms_.count(member.room) != 0;
}

bool RelayServer::tell_rooms_who_left() {
  bool told = false;
  for (const auto& member : members_) {
    if (!member->left_untold()) continue;
    member->farewelled = true;
    if (!missed_by_a_room(*member)) continue;
    tell_room(member->room, member->seat(), {{kKind, kGone}, {kFrom, member->seat()}});
    told = true;
  }
  return told;
}

void RelayServer::tell_room(const std::string& room, int except, const nlohmann::json& message) {
  for (Member* member : members_of(room)) {
    if (member->peer != except) member->connection.send(message);
  }
}

const std::vector<RelayServer::Member*>& RelayServer::entered(const std::string& room) const {
  static const std::vector<Member*> kNobody;
  const auto found = rooms_.find(room);
  return found != rooms_.end() ? found->second : kNobody;
}

std::vector<RelayServer::Member*> RelayServer::members_of(const std::string& room) {
  std::vector<Member*> found;
  for (Member* member : entered(room)) {
    if (member->inside(room)) found.push_back(member);
  }
  return found;
}

void RelayServer::place_in(Member& member, const std::string& code) {
  member.room = code;
  rooms_[code].push_back(&member);
}

void RelayServer::take_out_of_room(const Member& member) {
  const auto entered = rooms_.find(member.room);
  if (entered == rooms_.end()) return;  // (its room closed first)
  std::vector<Member*>& inside = entered->second;
  inside.erase(std::remove(inside.begin(), inside.end(), &member), inside.end());
}

}  // namespace redgem::net
