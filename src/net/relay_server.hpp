#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "net/frame_buffer.hpp"
#include "net/socket.hpp"
#include "net/wire.hpp"

namespace redgem::net {

// The port a relay listens on unless told otherwise.
inline constexpr std::uint16_t kDefaultRelayPort = 7778;

// A meeting point on the internet for games whose players cannot reach each other directly
// (home routers let connections out, not in). Every machine connects out to the relay; the
// first opens a room and gets a short code, the others join with the code, and the relay passes
// their messages on. It does for a room what a host does for the machines that joined it, so
// the machines talk to it exactly as guests talk to a host. `redgem relay` runs one.
// It faces the internet, so it trusts nobody: what one address may open, try and pile up is
// limited, silent machines are let go, and a room ends with whoever opened it.
class RelayServer {
 public:
  bool start(std::uint16_t port, std::string& error);
  std::uint16_t port() const { return port_; }
  // Moves messages in and out; `seconds` since the last call.
  void poll(float seconds);

  std::size_t room_count() const { return rooms_.size(); }
  std::size_t connection_count() const { return members_.size(); }

 private:
  // A machine connected to the relay: in no room until it said which.
  struct Member {
    Socket socket;
    FrameBuffer incoming;
    std::vector<std::uint8_t> outgoing;
    std::string address;  // where it connected from
    std::string room;
    int peer = 0;
    float waiting_for = 0.0f;  // seconds without saying which room
    float quiet_for = 0.0f;    // seconds since anything arrived from it
    float since_said = 0.0f;   // seconds since anything was sent to it
    bool closed = false;
    bool farewelled = false;  // the others in its room were told it is gone
  };
  // What is remembered about an address: its wrong guesses at room codes (forgotten with time),
  // and for how long it is kept out for guessing too much.
  struct Visitor {
    float wrong_codes = 0.0f;
    float kept_out_for = 0.0f;
  };

  void accept_newcomers();
  void forgive_visitors(float seconds);
  void keep_in_touch(Member& member, float seconds);
  int connections_from(const std::string& address) const;
  int rooms_opened_by(const std::string& address) const;
  // One member's turn in a poll: what arrived is read and handled, what waits is sent.
  void serve(Member& member, float seconds);
  void await_room_choice(Member& member, float seconds);
  void pump(Member& member);
  void handle(Member& member, const std::string& text);
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
  void queue(Member& member, const nlohmann::json& message);
  void tell_room(const std::string& room, int except, const nlohmann::json& message);
  std::vector<Member*> members_of(const std::string& room);
  std::string new_room_code();

  Socket listener_;
  std::uint16_t port_ = 0;
  std::vector<std::unique_ptr<Member>> members_;
  std::map<std::string, bool> rooms_;  // the codes of the open rooms
  std::map<std::string, Visitor> visitors_;
  std::mt19937 random_{std::random_device{}()};
};

}  // namespace redgem::net
