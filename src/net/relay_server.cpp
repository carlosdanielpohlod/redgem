#include "net/relay_server.hpp"

#include <algorithm>
#include <iterator>
#include <string_view>

namespace redgem::net {

namespace {

using namespace wire;

constexpr std::size_t kMaxConnections = 1024;
constexpr std::size_t kMaxRooms = 256;
// What one address may do. (Players in one house share an address: a whole party fits.)
constexpr int kMaxConnectionsPerAddress = 24;
constexpr int kMaxRoomsPerAddress = 8;
constexpr float kMaxWrongCodes = 10.0f;           // wrong guesses at a room code before being kept out ...
constexpr float kKeptOutSeconds = 120.0f;         // ... for this long
constexpr float kForgetWrongCodeSeconds = 30.0f;  // one wrong guess is forgotten after this
// What may wait to be sent to one machine: two of the longest messages. A relay holds many
// machines' queues at once, so it is far stricter than a game is with its few.
constexpr std::size_t kMaxPendingPerMember = 2 * kMaxFrameBytes;
// Letters and digits that are not mistaken for one another when read aloud or typed (no I, O, 0, 1).
constexpr std::string_view kCodeAlphabet = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
constexpr int kCodeTries = 64;  // at finding a code no open room has

}  // namespace

bool RelayServer::has_opener(const std::vector<Member*>& inside) {
  return std::any_of(inside.begin(), inside.end(),
                     [](const Member* member) { return member->peer == kHostId && !member->closed; });
}

bool RelayServer::start(std::uint16_t port, std::string& error) {
  listener_ = Socket::listen_on(port, error);
  if (!listener_.valid()) return false;
  port_ = listener_.local_port();
  return true;
}

void RelayServer::poll(float seconds) {
  if (!listener_.valid()) return;
  forgive_visitors(seconds);
  accept_newcomers();
  // By index: nothing below adds members, but entering a room touches other members.
  for (std::size_t i = 0; i < members_.size(); ++i) serve(*members_[i], seconds);
  drop_closed_members();
}

void RelayServer::serve(Member& member, float seconds) {
  if (member.peer == 0) await_room_choice(member, seconds);
  pump(member);
  if (member.peer != 0) keep_in_touch(member, seconds);
  while (const auto text = member.incoming.next()) {
    if (member.closed) break;
    handle(member, *text);
  }
  if (member.incoming.broken()) member.closed = true;
}

void RelayServer::await_room_choice(Member& member, float seconds) {
  member.waiting_for += seconds;
  if (member.waiting_for > kJoinTimeoutSeconds) member.closed = true;  // connected and said nothing
}

void RelayServer::accept_newcomers() {
  for (Socket guest = listener_.accept(); guest.valid(); guest = listener_.accept()) {
    const std::string address = guest.peer_address();
    const auto visitor = visitors_.find(address);
    const bool kept_out = visitor != visitors_.end() && visitor->second.kept_out_for > 0.0f;
    // Full, kept out for guessing, or too many from one place: hung up on.
    if (members_.size() >= kMaxConnections || kept_out || connections_from(address) >= kMaxConnectionsPerAddress)
      continue;
    auto member = std::make_unique<Member>();
    member->socket = std::move(guest);
    member->address = address;
    members_.push_back(std::move(member));
  }
}

void RelayServer::forgive_visitors(float seconds) {
  for (auto visitor = visitors_.begin(); visitor != visitors_.end();) {
    Visitor& record = visitor->second;
    record.kept_out_for = std::max(0.0f, record.kept_out_for - seconds);
    record.wrong_codes = std::max(0.0f, record.wrong_codes - seconds / kForgetWrongCodeSeconds);
    visitor = record.kept_out_for <= 0.0f && record.wrong_codes <= 0.0f ? visitors_.erase(visitor) : std::next(visitor);
  }
}

void RelayServer::keep_in_touch(Member& member, float seconds) {
  member.quiet_for += seconds;
  member.since_said += seconds;
  if (member.quiet_for > kSilenceTimeoutSeconds) {
    member.closed = true;  // gone without a goodbye
  } else if (member.since_said >= kPingEverySeconds) {
    queue(member, {{kKind, kPing}});
  }
}

int RelayServer::connections_from(const std::string& address) const {
  return static_cast<int>(std::count_if(members_.begin(), members_.end(), [&address](const auto& member) {
    return !member->closed && member->address == address;
  }));
}

int RelayServer::rooms_opened_by(const std::string& address) const {
  return static_cast<int>(std::count_if(members_.begin(), members_.end(), [&address](const auto& member) {
    return !member->closed && member->peer == kHostId && member->address == address;
  }));
}

void RelayServer::pump(Member& member) {
  std::vector<std::uint8_t> arrived;
  if (member.socket.read(arrived, kMaxReadBytesPerPoll) == IoStatus::Closed) member.closed = true;
  if (!arrived.empty()) member.quiet_for = 0.0f;
  member.incoming.append(arrived.data(), arrived.size());
  if (!member.closed && member.socket.write(member.outgoing) == IoStatus::Closed) member.closed = true;
}

void RelayServer::handle(Member& member, const std::string& text) {
  const nlohmann::json message = nlohmann::json::parse(text, nullptr, false);
  if (!well_formed(message)) {
    member.closed = true;
    return;
  }
  try {
    act_on(member, message);
  } catch (const nlohmann::json::exception&) {
    member.closed = true;  // (well_formed should have caught it: one machine never takes the relay down)
  }
}

void RelayServer::act_on(Member& member, const nlohmann::json& message) {
  if (member.peer == 0) {
    hear_newcomer(member, message);
  } else if (message[kKind] == kGameMessage && message.contains(kName)) {
    pass_on(member, message);
  }
}

void RelayServer::hear_newcomer(Member& member, const nlohmann::json& message) {
  // A newcomer's first words say which room; anything else and it is not let in.
  if (message[kKind] != kHello || !message.contains(kRoom)) {
    member.closed = true;
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

void RelayServer::open_room(Member& member) {
  const bool room_to_give = rooms_.size() < kMaxRooms && rooms_opened_by(member.address) < kMaxRoomsPerAddress;
  const std::string code = room_to_give ? new_room_code() : std::string();
  if (code.empty()) {
    member.closed = true;
    return;
  }
  rooms_[code] = true;
  member.room = code;
  member.peer = kHostId;
  queue(member, {{kKind, kWelcome}, {kTo, member.peer}, {kPeers, std::vector<int>{}}, {kRoom, code}});
}

void RelayServer::note_wrong_code(Member& member) {
  // A wrong code may be a typo; many are someone trying every code there is.
  Visitor& visitor = visitors_[member.address];
  visitor.wrong_codes += 1.0f;
  if (visitor.wrong_codes >= kMaxWrongCodes) visitor.kept_out_for = kKeptOutSeconds;
  member.closed = true;
}

void RelayServer::join_room(Member& member, const std::string& room) {
  std::vector<int> ids;
  for (const Member* other : members_of(room)) ids.push_back(other->peer);
  const int id = free_peer_id(ids);
  if (id == 0) {
    member.closed = true;  // a full room
    return;
  }
  member.room = room;
  member.peer = id;
  queue(member, {{kKind, kWelcome}, {kTo, member.peer}, {kPeers, ids}, {kRoom, room}});
  tell_room(room, member.peer, {{kKind, kArrived}, {kFrom, member.peer}});
}

void RelayServer::pass_on(Member& sender, const nlohmann::json& message) {
  const int to = message.value(kTo, kEveryone);
  // Who it is from is what the relay knows, not what the message claims.
  const nlohmann::json passed_on =
      game_message(message[kName], message.value(kData, nlohmann::json()), sender.peer, to);
  for (Member* other : members_of(sender.room)) {
    if (other->peer != sender.peer && (to == kEveryone || other->peer == to)) queue(*other, passed_on);
  }
}

void RelayServer::drop_closed_members() {
  // Telling a room that someone left can itself find another member gone (and that one may have
  // been the room's opener): go round until nothing more changes.
  for (bool changed = true; changed;) {
    close_rooms_without_opener();
    changed = tell_rooms_who_left();
  }
  members_.erase(std::remove_if(members_.begin(), members_.end(),
                                [](const std::unique_ptr<Member>& member) { return member->closed; }),
                 members_.end());
}

void RelayServer::close_rooms_without_opener() {
  // A room ends with the machine that opened it: everyone in it is let go.
  for (auto room = rooms_.begin(); room != rooms_.end();) {
    const std::vector<Member*> inside = members_of(room->first);
    if (has_opener(inside)) {
      ++room;
      continue;
    }
    for (Member* member : inside) member->closed = true;
    room = rooms_.erase(room);
  }
}

bool RelayServer::tell_rooms_who_left() {
  bool told = false;
  for (const auto& member : members_) {
    if (!member->closed || member->farewelled) continue;
    member->farewelled = true;
    if (member->peer < kFirstGuestId || rooms_.count(member->room) == 0) continue;
    tell_room(member->room, member->peer, {{kKind, kGone}, {kFrom, member->peer}});
    told = true;
  }
  return told;
}

void RelayServer::queue(Member& member, const nlohmann::json& message) {
  const std::vector<std::uint8_t> bytes = packed(message);
  member.outgoing.insert(member.outgoing.end(), bytes.begin(), bytes.end());
  member.since_said = 0.0f;
  if (member.outgoing.size() > kMaxPendingPerMember) {
    member.closed = true;  // it stopped listening
    member.outgoing.clear();
    return;
  }
  if (member.socket.write(member.outgoing) == IoStatus::Closed) member.closed = true;
}

void RelayServer::tell_room(const std::string& room, int except, const nlohmann::json& message) {
  for (Member* member : members_of(room)) {
    if (member->peer != except && !member->closed) queue(*member, message);
  }
}

std::vector<RelayServer::Member*> RelayServer::members_of(const std::string& room) {
  std::vector<Member*> found;
  for (const auto& member : members_) {
    if (member->peer != 0 && !member->closed && member->room == room) found.push_back(member.get());
  }
  return found;
}

std::string RelayServer::new_room_code() {
  std::uniform_int_distribution<std::size_t> letter(0, kCodeAlphabet.size() - 1);
  for (int attempt = 0; attempt < kCodeTries; ++attempt) {
    std::string code;
    for (std::size_t i = 0; i < kRoomCodeLetters; ++i) code.push_back(kCodeAlphabet[letter(random_)]);
    if (rooms_.count(code) == 0) return code;
  }
  return {};
}

}  // namespace redgem::net
