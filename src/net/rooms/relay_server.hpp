#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "net/byte_allowance.hpp"
#include "net/rooms/code_guessers.hpp"
#include "net/connection.hpp"
#include "net/rooms/room_codes.hpp"
#include "net/socket/socket.hpp"
#include "net/wire.hpp"

namespace redgem::net {

// The port a relay listens on unless told otherwise.
inline constexpr std::uint16_t kDefaultRelayPort = 7778;
// What a relay holds for all its machines together, of what arrived and of what waits to be sent.
// Each machine's own limits, times the machines there may be, would be far more memory than a
// small server has.
inline constexpr std::size_t kMaxRelayBufferedBytes = 256 * 1024 * 1024;
// What one machine may send through a relay: this much a second over time, and at once as much as
// a few of the longest messages. Far more than a game says; a machine past it is let go.
inline constexpr float kRelayBytesPerSecond = 1024.0f * 1024.0f;
inline constexpr float kRelayBurstBytes = 4.0f * kRelayBytesPerSecond;
// What each message costs on top of its bytes: passing one on is work whatever its size, so a
// machine sending floods of tiny ones runs out as one sending big ones does. (Poses of dozens of
// objects at every frame stay far below it.)
inline constexpr std::size_t kRelayMessageCostBytes = 64;
// Machines a relay serves at once, and rooms it holds.
inline constexpr std::size_t kMaxRelayConnections = 1024;
inline constexpr std::size_t kMaxRelayRooms = 256;
// The rooms one address may have open on a relay unless it is told otherwise.
inline constexpr int kDefaultRoomsPerAddress = 8;
// How long a relay that could not take a newcomer (the system had no connection left to give)
// leaves the ones waiting alone before it tries again.
inline constexpr float kAcceptRestSeconds = 0.25f;
// How long a newcomer may wait for the game of a room it asked into to decide on it: the game's
// own time to decide, and a little more for its refusal to come through first.
inline constexpr float kRelayApprovalSeconds = kApprovalSeconds + 2.0f;

// What one address may hold on a relay: machines connected, and rooms open (redgem-relay's
// --per-address and --rooms-per-address).
struct RelayLimits {
  int connections_per_address = kDefaultConnectionsPerAddress;
  int rooms_per_address = kDefaultRoomsPerAddress;
};

// A meeting point on the internet for games whose players cannot reach each other directly
// (home routers let connections out, not in). Every machine connects out to the relay; the
// first opens a room and gets a short code, the others join with the code, and the relay passes
// their messages on. It does for a room what a host does for the machines that joined it, so
// the machines talk to it exactly as guests talk to a host. The redgem-relay program runs one
// (net/rooms/relay_program.hpp).
// It faces the internet, so it trusts nobody: what one address may open, try and pile up is
// limited, silent machines are let go, and a room ends with whoever opened it.
// When the game of a room decides who comes in (Network.authenticate), the relay decides
// nothing: it passes what a newcomer's join carries to the room's opener, and the opener's
// decision back (relay_server_approval.cpp).
class RelayServer {
 public:
  RelayServer() = default;
  // A relay that holds at most `buffered_bytes` for all its machines (a small server's may be less).
  explicit RelayServer(std::size_t buffered_bytes) : max_buffered_bytes_(buffered_bytes) {}
  // A relay that lets each address hold what `limits` say.
  explicit RelayServer(const RelayLimits& limits) : limits_(limits) {}

  bool start(std::uint16_t port, std::string& error);
  std::uint16_t port() const { return port_; }
  // Moves messages in and out; `seconds` since the last call.
  void poll(float seconds);
  // Sleeps until there is something for poll to do: someone connects, a machine says something
  // or hangs up, or one that could not be sent to can again; for `seconds` at most (what poll
  // does by the clock, letting silent machines go and asking the quiet ones if they are there,
  // still needs calling now and then). True when it was not the time that ended the wait.
  bool wait(float seconds) const;

  std::size_t room_count() const { return rooms_.size(); }
  std::size_t connection_count() const { return members_.size(); }

 private:
  // A machine connected to the relay: in no room until it said which.
  struct Member {
    Connection connection;
    std::string room;
    int peer = 0;
    ByteAllowance allowance{kRelayBytesPerSecond, kRelayBurstBytes};
    bool farewelled = false;  // the others in its room were told it is gone
    bool guarded = false;     // it opened its room, whose game decides who comes in
    int asking_as = 0;        // the id it would have, while the game of its room decides on it
    float asking_for = 0.0f;

    bool in_room() const { return peer != 0; }
    bool asking() const { return asking_as != 0; }
    // The id it holds in its room: its own, or the one it would have.
    int seat() const { return in_room() ? peer : asking_as; }
    // Holding a place in room `code` (in it, or waiting to be let in). One that left holds it until
    // the room was told: its id is nobody else's before then, or the room would hear of the
    // newcomer and then that the id left.
    bool holds_seat_in(const std::string& code) const { return seat() != 0 && !farewelled && room == code; }
    // Still there, waiting in room `code` for its game to decide on it as `id`.
    bool asks_as(const std::string& code, int id) const {
      return asking() && asking_as == id && !connection.closed() && room == code;
    }
    bool connected_from(const std::string& address) const {
      return !connection.closed() && connection.address() == address;
    }
    // Whether it opened a room and still holds it: a room ends with whoever opened it.
    bool holds_room() const { return peer == kHostId && !connection.closed(); }
    bool inside(const std::string& code) const { return in_room() && !connection.closed() && room == code; }
    // Gone, with the others in its room not told yet.
    bool left_untold() const { return connection.closed() && !farewelled; }
  };
  void accept_newcomers();
  // Whether one more machine from `address` may connect.
  bool room_for(const std::string& address) const;
  int connections_from(const std::string& address) const;
  int rooms_opened_by(const std::string& address) const;
  // One member's turn in a poll: what arrived is read and handled, what waits is sent.
  void serve(Member& member, float seconds);
  // What time passing does to a member not in a room yet: given up on when it keeps quiet, or
  // when its room's game does not decide on it.
  void wait_on(Member& member, float seconds);
  // Takes what arrived out of what the member may send; over it, the member is let go (false).
  bool charge(Member& member, std::size_t bytes);
  // Lets go of the newest machines with much waiting, while the relay holds more than it may.
  void shed_load();
  void handle(Member& member, const nlohmann::json& message);
  void act_on(Member& member, const nlohmann::json& message);
  void hear_newcomer(Member& member, const nlohmann::json& message);
  void enter_room(Member& member, const std::string& wanted, const nlohmann::json& auth);
  void open_room(Member& member);
  void note_wrong_code(Member& member);
  void join_room(Member& member, const std::string& room, const nlohmann::json& auth);
  // Whether a room can take a newcomer: it has a place left (`free_id`), and is not ending (its
  // opener just left: the room closes at the end of this poll).
  static bool takes_newcomers(int free_id, const Member* opener);
  void let_in(Member& member, int id);
  // Connection approval (relay_server_approval.cpp).
  void ask(Member& member, Member& opener, int id, const nlohmann::json& auth);
  void hear_opener(Member& opener, const nlohmann::json& message);
  void settle(const std::string& room, const nlohmann::json& verdict);
  static void await_verdict(Member& member, float seconds);
  Member* asking_member(const std::string& room, int id);
  Member* opener_of(const std::string& room);
  // The ids of a room's members and of the newcomers its game is deciding on.
  std::vector<int> seats_taken(const std::string& room) const;
  void pass_on(Member& sender, const nlohmann::json& message);
  void drop_closed_members();
  void close_rooms_without_opener();
  // Lets go of everyone holding a place in a room whose opener left.
  void empty_room(const std::string& room);
  static bool has_opener(const std::vector<Member*>& inside);
  // Tells each room which of its members just went; true when some room was told.
  bool tell_rooms_who_left();
  // Whether a member that left held a guest's place in a room that is still open: someone is
  // there to tell.
  bool missed_by_a_room(const Member& member) const;
  void tell_room(const std::string& room, int except, const nlohmann::json& message);
  std::vector<Member*> members_of(const std::string& room);
  // Whoever entered a room and was not dropped yet (gone or not); nobody for a room not open.
  const std::vector<Member*>& entered(const std::string& room) const;
  // Puts a member in room `code`, and in its room's list.
  void place_in(Member& member, const std::string& code);
  // Takes a member that is being dropped out of its room's list.
  void take_out_of_room(const Member& member);
  // A code no open room has; empty when none was found.
  std::string new_room_code() const;

  Socket listener_;
  std::uint16_t port_ = 0;
  std::vector<std::unique_ptr<Member>> members_;
  // The open rooms by code, each with whoever entered it: what a message passed on looks through,
  // instead of every machine on the relay. (Those who went are taken out as they are dropped.)
  std::map<std::string, std::vector<Member*>> rooms_;
  CodeGuessers guessers_;
  std::size_t max_buffered_bytes_ = kMaxRelayBufferedBytes;
  RelayLimits limits_;
  float accept_rest_ = 0.0f;  // seconds until newcomers are taken again (see kAcceptRestSeconds)
};

}  // namespace redgem::net
