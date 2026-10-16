// RelayServer: its rooms. Whoever asks for a new one opens it and gets its code, the others
// enter with the code, and a room ends with the machine that opened it.

#include "net/rooms/relay_server.hpp"

#include <algorithm>

namespace redgem::net {

using namespace wire;

namespace {

constexpr std::size_t kMaxRooms = 256;
constexpr int kMaxRoomsPerAddress = 8;  // what one address may open

// A newcomer's first words say which room it wants.
bool asks_for_a_room(const nlohmann::json& message) { return message[kKind] == kHello && message.contains(kRoom); }

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
  enter_room(member, message[kRoom]);
}

void RelayServer::enter_room(Member& member, const std::string& wanted) {
  if (wanted == kNewRoom) {
    open_room(member);
  } else if (rooms_.count(wanted) == 0) {
    note_wrong_code(member);
  } else {
    join_room(member, wanted);
  }
}

int RelayServer::rooms_opened_by(const std::string& address) const {
  return static_cast<int>(std::count_if(members_.begin(), members_.end(), [&address](const auto& member) {
    return member->holds_room() && member->connected_from(address);
  }));
}

void RelayServer::open_room(Member& member) {
  const bool room_to_give =
      rooms_.size() < kMaxRooms && rooms_opened_by(member.connection.address()) < kMaxRoomsPerAddress;
  const std::string code = room_to_give ? new_room_code() : std::string();
  if (code.empty()) {
    member.connection.close();
    return;
  }
  rooms_[code] = true;
  member.room = code;
  member.peer = kHostId;
  member.connection.send({{kKind, kWelcome}, {kTo, member.peer}, {kPeers, std::vector<int>{}}, {kRoom, code}});
}

void RelayServer::join_room(Member& member, const std::string& room) {
  std::vector<int> ids;
  for (const Member* other : members_of(room)) ids.push_back(other->peer);
  const int id = free_peer_id(ids);
  if (id == 0) {
    member.connection.close();  // a full room
    return;
  }
  member.room = room;
  member.peer = id;
  member.connection.send({{kKind, kWelcome}, {kTo, member.peer}, {kPeers, ids}, {kRoom, room}});
  tell_room(room, member.peer, {{kKind, kArrived}, {kFrom, member.peer}});
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
    for (Member* member : inside) member->connection.close();
    room = rooms_.erase(room);
  }
}

bool RelayServer::missed_by_a_room(const Member& member) const {
  return member.peer >= kFirstGuestId && rooms_.count(member.room) != 0;
}

bool RelayServer::tell_rooms_who_left() {
  bool told = false;
  for (const auto& member : members_) {
    if (!member->left_untold()) continue;
    member->farewelled = true;
    if (!missed_by_a_room(*member)) continue;
    tell_room(member->room, member->peer, {{kKind, kGone}, {kFrom, member->peer}});
    told = true;
  }
  return told;
}

void RelayServer::tell_room(const std::string& room, int except, const nlohmann::json& message) {
  for (Member* member : members_of(room)) {
    if (member->peer != except) member->connection.send(message);
  }
}

std::vector<RelayServer::Member*> RelayServer::members_of(const std::string& room) {
  std::vector<Member*> found;
  for (const auto& member : members_) {
    if (member->inside(room)) found.push_back(member.get());
  }
  return found;
}

}  // namespace redgem::net
