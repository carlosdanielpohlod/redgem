#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "net/byte_allowance.hpp"
#include "net/code_guessers.hpp"
#include "net/connection.hpp"
#include "net/room_codes.hpp"
#include "net/socket.hpp"
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
// Machines a relay serves at once.
inline constexpr std::size_t kMaxRelayConnections = 1024;
// How long a relay that could not take a newcomer (the system had no connection left to give)
// leaves the ones waiting alone before it tries again.
inline constexpr float kAcceptRestSeconds = 0.25f;

// A meeting point on the internet for games whose players cannot reach each other directly
// (home routers let connections out, not in). Every machine connects out to the relay; the
// first opens a room and gets a short code, the others join with the code, and the relay passes
// their messages on. It does for a room what a host does for the machines that joined it, so
// the machines talk to it exactly as guests talk to a host. The redgem-relay program runs one
// (net/relay_program.hpp).
// It faces the internet, so it trusts nobody: what one address may open, try and pile up is
// limited, silent machines are let go, and a room ends with whoever opened it.
class RelayServer {
 public:
  RelayServer() = default;
  // A relay that holds at most `buffered_bytes` for all its machines (a small server's may be less).
  explicit RelayServer(std::size_t buffered_bytes) : max_buffered_bytes_(buffered_bytes) {}

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

    bool in_room() const { return peer != 0; }
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
  // Takes what arrived out of what the member may send; over it, the member is let go.
  void charge(Member& member, std::size_t bytes);
  // Lets go of the newest machines with much waiting, while the relay holds more than it may.
  void shed_load();
  void handle(Member& member, const nlohmann::json& message);
  void act_on(Member& member, const nlohmann::json& message);
  void hear_newcomer(Member& member, const nlohmann::json& message);
  void enter_room(Member& member, const std::string& wanted);
  void open_room(Member& member);
  void note_wrong_code(Member& member);
  void join_room(Member& member, const std::string& room);
  void pass_on(Member& sender, const nlohmann::json& message);
  void drop_closed_members();
  void close_rooms_without_opener();
  static bool has_opener(const std::vector<Member*>& inside);
  // Tells each room which of its members just went; true when some room was told.
  bool tell_rooms_who_left();
  // Whether a member that left was a guest of a room that is still open: someone is there to tell.
  bool missed_by_a_room(const Member& member) const;
  void tell_room(const std::string& room, int except, const nlohmann::json& message);
  std::vector<Member*> members_of(const std::string& room);
  // A code no open room has; empty when none was found.
  std::string new_room_code() const;

  Socket listener_;
  std::uint16_t port_ = 0;
  std::vector<std::unique_ptr<Member>> members_;
  std::map<std::string, bool> rooms_;  // the codes of the open rooms
  CodeGuessers guessers_;
  std::size_t max_buffered_bytes_ = kMaxRelayBufferedBytes;
  float accept_rest_ = 0.0f;  // seconds until newcomers are taken again (see kAcceptRestSeconds)
};

}  // namespace redgem::net
