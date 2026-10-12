#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "net/connection.hpp"
#include "net/socket.hpp"
#include "net/wire.hpp"

namespace redgem::net {

struct NetworkEvent {
  enum class Kind {
    Connected,     // this machine is in the game (hosting, or its join was accepted)
    Failed,        // a join did not work out
    Disconnected,  // the game is gone (the host left, or the connection dropped)
    PeerJoined,    // someone else arrived
    PeerLeft,      // someone else is gone
    Message        // `peer` sent `name` with `data`
  };
  Kind kind = Kind::Message;
  int peer = 0;
  std::string name;
  nlohmann::json data;
};

// One multiplayer game over TCP (Network in Ruby). One machine hosts and the others join it;
// every message goes through the host, which passes it on. Messages arrive whole and in the order
// they were sent. Nothing here waits: call poll() every frame.
// When the machines cannot reach each other directly they meet in a room on a relay (see
// RelayServer): whoever opens the room is the game's host in every way but one, the relay does
// the passing on.
class NetworkSession {
 public:
  enum class State { Offline, Connecting, Online };

  // Starts a game others can join on `port` (0 picks a free one, see port()).
  bool host(std::uint16_t port, std::string& error);
  // Starts joining the game hosted at `address`. A Connected or Failed event tells how it went.
  bool join(const std::string& address, std::uint16_t port, std::string& error);
  // Opens a room on the relay at `address`; the Connected event comes with its code in room().
  bool host_room(const std::string& address, std::uint16_t port, std::string& error);
  // Joins the room with that code on the relay at `address`.
  bool join_room(const std::string& address, std::uint16_t port, const std::string& code, std::string& error);
  // The code of the room this game is in, on a relay; empty for a game hosted directly.
  const std::string& room() const { return room_; }
  // Leaves the game (ending it for everyone when hosting).
  void leave();

  // Moves messages in and out. `seconds` since the last call, for giving up on a join.
  std::vector<NetworkEvent> poll(float seconds);
  // Sends a message to everyone else (kEveryone) or to one peer. False when offline or when there
  // is no such peer.
  bool send(const std::string& name, const nlohmann::json& data, int to = kEveryone);

  State state() const { return state_; }
  bool hosting() const { return state_ == State::Online && id_ == kHostId; }
  // This machine's id in the game; 0 while not in one.
  int id() const { return state_ == State::Online ? id_ : 0; }
  // Everyone else's ids.
  const std::vector<int>& peers() const { return peers_; }
  // The port being hosted on, or of the game joined.
  std::uint16_t port() const { return port_; }

 private:
  // A connection: the host's to each machine that joined, a joiner's to the host.
  struct Link {
    Connection connection;
    int peer = 0;  // 0 on the host until the newcomer said hello and was let in
  };

  bool connect_out(const std::string& address, std::uint16_t port, const nlohmann::json& hello, std::string& error);
  // Whether this machine passes the others' messages on itself (a host reached directly).
  bool relaying() const { return id_ == kHostId && !through_relay_; }
  void announce_hosting(std::vector<NetworkEvent>& events);
  // One link's turn in a poll: what arrived is read and handled, what waits is sent.
  void serve(Link& link, float seconds, std::vector<NetworkEvent>& events);
  void receive_newcomers(float seconds);
  void accept_newcomers();
  // Whether one more machine from `address` may wait to say hello.
  bool room_to_wait(const std::string& address) const;
  void admit(Link& link, std::vector<NetworkEvent>& events);
  void drop_silent_newcomers(float seconds);
  void await_connection(float seconds, std::vector<NetworkEvent>& events);
  void handle(Link& link, const nlohmann::json& message, std::vector<NetworkEvent>& events);
  void handle_as_host(Link& link, const nlohmann::json& message, std::vector<NetworkEvent>& events);
  void hear_newcomer(Link& link, const nlohmann::json& message, std::vector<NetworkEvent>& events);
  // Sends a peer's message on to whom it is for; true when that includes this machine.
  bool pass_on(int from, int to, const nlohmann::json& message);
  void handle_as_guest(const nlohmann::json& message, std::vector<NetworkEvent>& events);
  void accept_welcome(const nlohmann::json& message, std::vector<NetworkEvent>& events);
  void hear_game_message(const nlohmann::json& message, std::vector<NetworkEvent>& events);
  void peer_arrived(int peer, std::vector<NetworkEvent>& events);
  void peer_left(int peer, std::vector<NetworkEvent>& events);
  void drop_closed_links(std::vector<NetworkEvent>& events);
  void end_game_if_host_gone(std::vector<NetworkEvent>& events);
  // Forgets the links that closed; whose they were, in the order they were held (0: never let in).
  std::vector<int> take_closed_links();
  bool send_through_host(const nlohmann::json& message, int to);
  bool send_as_host(const nlohmann::json& message, int to);
  void queue_for_everyone_but(int peer, const nlohmann::json& message);
  Link* link_to(int peer);
  void go_offline();

  State state_ = State::Offline;
  int id_ = 0;
  std::uint16_t port_ = 0;
  float connecting_for_ = 0.0f;
  bool announce_hosting_ = false;
  bool through_relay_ = false;  // the game is a room on a relay
  bool opening_room_ = false;   // ... which this machine asked to open
  std::string room_;
  Socket listener_;
  std::vector<Link> links_;
  std::vector<int> peers_;
};

}  // namespace redgem::net
