#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "net/byte_allowance.hpp"
#include "net/connection.hpp"
#include "net/socket/socket.hpp"
#include "net/wire.hpp"

namespace redgem::net {

// What a machine in a dedicated server's room may send: a game's talk many times over, far less
// than a flood (on a server, every message it sends is work for the game's scripts); and at once,
// one of the longest messages and more.
inline constexpr float kServedBytesPerSecond = 256.0f * 1024.0f;
inline constexpr float kServedBurstBytes = 2.0f * kMaxFrameBytes;
// A machine in a dedicated server's room that says nothing to the game for this long (only "I am
// here") is let go: a place in a room is for playing.
inline constexpr float kServedIdleSeconds = 300.0f;

struct NetworkEvent {
  enum class Kind {
    Connected,     // this machine is in the game (hosting, or its join was accepted)
    Failed,        // a join did not work out
    Disconnected,  // the game is gone (the host left, or the connection dropped)
    PeerJoined,    // someone else arrived
    PeerLeft,      // someone else is gone
    Message,       // `peer` sent `name` with `data`
    Approval       // a newcomer asks to come in as `peer`, with `data`: the game decides (see decide)
  };
  Kind kind = Kind::Message;
  int peer = 0;
  std::string name;  // a message's; a Failed join's reason, when the host gave one
  nlohmann::json data;
  int ticket = 0;  // an Approval's: what the decision names

  // Something that happened, to or about `peer`.
  static NetworkEvent of(Kind kind, int peer = 0);
  // What `from` said.
  static NetworkEvent message(int from, const std::string& name, const nlohmann::json& data);
  // A newcomer who would be `peer` asks to come in, with what it carries for the game to decide on.
  static NetworkEvent approval(int peer, int ticket, const nlohmann::json& auth);
};

// One multiplayer game over TCP (Network in Ruby). One machine hosts and the others join it;
// every message goes through the host, which passes it on. Messages arrive whole and in the order
// they were sent. Nothing here waits: call poll() every frame.
// When the machines cannot reach each other directly they meet in a room on a relay (see
// RelayServer): whoever opens the room is the game's host in every way but one, the relay does
// the passing on. On a dedicated server (redgem-server) each room is a game the server hosts,
// with no player of its own: the server hands the room the machines that asked for it.
class NetworkSession {
 public:
  enum class State { Offline, Connecting, Online };

  // Starts a game others can join on `port` (0 picks a free one, see port()).
  bool host(std::uint16_t port, std::string& error);
  // Starts joining the game hosted at `address`. A Connected or Failed event tells how it went.
  // `auth` (a hash, or null) goes with the hello, for the game joined to decide on.
  bool join(const std::string& address, std::uint16_t port, std::string& error, const nlohmann::json& auth = nullptr);
  // Opens a room on the relay at `address`; the Connected event comes with its code in room().
  bool host_room(const std::string& address, std::uint16_t port, std::string& error);
  // Joins the room with that code on the relay at `address`.
  bool join_room(const std::string& address, std::uint16_t port, const std::string& code, std::string& error,
                 const nlohmann::json& auth = nullptr);
  // Starts a game as a room of a dedicated server, under the room's code: this machine is the
  // host, with no player of its own.
  void serve_room(const std::string& code);
  // Lets a machine that asked the dedicated server for this room in: it is told its id, and the
  // others of its arrival (with the next poll's events). With no place left it is let go. When
  // the game decides who comes in, it is asked first, with `auth` from the machine's hello.
  void admit_arrival(Connection connection, const nlohmann::json& auth = nullptr);
  // Connection approval, on the machine hosting (directly, or a dedicated server's room): with
  // it, a newcomer is not let in until the game decides (an Approval event, then decide()), and is
  // turned away when the game has not decided within kApprovalSeconds. Without it everyone is let
  // in. It lasts while hosting other games, until turned off.
  void require_approval(bool required) { approving_ = required; }
  // The game's decision on the newcomer of an Approval event, named by its ticket: let in, or
  // turned away with `reason` (cut to kMaxReasonBytes). False when there is nobody to decide on any
  // more: the newcomer left, was decided on already, or the time to decide ran out.
  bool decide(int ticket, bool accepted, const std::string& reason = {});
  // The code of the room this game is in, on a relay or a dedicated server; empty for a game
  // hosted directly.
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
  // Whether this machine is a dedicated server's room: the game's host, with no player.
  bool serving() const { return server_hosted_ && hosting(); }
  // The others who play: everyone else, without a dedicated server.
  std::vector<int> players() const;
  // How many newcomers the game is deciding on (connection approval).
  std::size_t deciding() const;
  // How many of the machines in the game connected from `address` (on the host).
  int links_from(const std::string& address) const;
  // The port being hosted on, or of the game joined.
  std::uint16_t port() const { return port_; }

 private:
  // A connection: the host's to each machine that joined, a joiner's to the host.
  struct Link {
    Connection connection;
    int peer = 0;  // 0 on the host until the newcomer said hello and was let in
    ByteAllowance allowance{kServedBytesPerSecond, kServedBurstBytes};  // on a dedicated server
    float idle_for = 0.0f;  // seconds since it last said something to the game, on a dedicated server
    int asking_as = 0;      // the id a newcomer would have, while the game decides on it
    int ticket = 0;         // ... and what the decision names
    float asking_for = 0.0f;

    bool let_in() const { return peer != 0; }
    bool asking() const { return asking_as != 0; }
    // A newcomer that has not said hello yet.
    bool before_hello() const { return !let_in() && !asking(); }
    // Whether it is the newcomer a decision with this ticket is about, and still there to hear it.
    bool asks_with(int number) const { return asking() && ticket == number && !connection.closed(); }
    // In the game, and not the machine that is `other` there.
    bool is_another_than(int other) const { return let_in() && peer != other; }
    // A newcomer that may still say hello.
    bool waiting() const { return !let_in() && !connection.closed(); }
    // Still there, and connected from `address`.
    bool connected_from(const std::string& address) const {
      return !connection.closed() && connection.address() == address;
    }
  };

  bool connect_out(const std::string& address, std::uint16_t port, const nlohmann::json& hello, std::string& error);
  // Whether this machine passes the others' messages on itself (a host reached directly).
  bool relaying() const { return id_ == kHostId && !through_relay_; }
  // Whether `peer` is one of the others in the game.
  bool knows(int peer) const;
  // Whether a message for `to` is one this machine, as the host, hears itself.
  static bool reaches_host(int to) { return to == kEveryone || to == kHostId; }
  bool offline() const { return state_ == State::Offline; }
  // Whether the link at that place gets its turn: there is one, and the game did not end before it.
  bool still_serving(std::size_t link) const;
  // Whether a link is one to keep in touch with: this machine is in the game, and so is the other.
  bool in_touch_with(const Link& link) const;
  // Online as the game's host, with the game told so at the next poll.
  void start_hosting();
  // One link's turn in a poll: what arrived is read and handled, what waits is sent.
  void serve(Link& link, float seconds, std::vector<NetworkEvent>& events);
  // What a dedicated server's room holds its machines to: one that sends more than it may, or
  // says nothing to the game for too long, is let go.
  void police(Link& link, std::size_t bytes, float seconds);
  // The newcomers' turn in a poll: those connecting are taken in, those the game is deciding on wait.
  void tend_newcomers(float seconds);
  void receive_newcomers(float seconds);
  void accept_newcomers();
  // Whether one more machine from `address` may wait to say hello.
  bool room_to_wait(const std::string& address) const;
  // A newcomer that said hello, with `auth`: let in, or put to the game when it decides who comes in.
  void consider(Link& link, const nlohmann::json& auth, std::vector<NetworkEvent>& events);
  void admit(Link& link, std::vector<NetworkEvent>& events);
  // Lets a newcomer in as `peer` (0: there is no place left, and it is let go).
  void let_in(Link& link, int peer, std::vector<NetworkEvent>& events);
  // Connection approval (network_session_approval.cpp).
  void ask(Link& link, const nlohmann::json& auth, std::vector<NetworkEvent>& events);
  static void refuse(Link& link, const std::string& reason);
  void wait_for_decisions(float seconds);
  Link* asking_link(int ticket);
  // The ids of the others in the game and of the newcomers the game is deciding on.
  std::vector<int> ids_taken() const;
  // What a newcomer let in as `peer` is told: its id and who else is here (and, on a dedicated
  // server, the room's code and that its host plays nobody).
  nlohmann::json welcome_for(int peer, const std::vector<int>& already_here) const;
  void drop_silent_newcomers(float seconds);
  void await_connection(float seconds, std::vector<NetworkEvent>& events);
  void handle(Link& link, const nlohmann::json& message, std::vector<NetworkEvent>& events);
  void handle_as_host(Link& link, const nlohmann::json& message, std::vector<NetworkEvent>& events);
  void hear_newcomer(Link& link, const nlohmann::json& message, std::vector<NetworkEvent>& events);
  // Sends a peer's message on to whom it is for; true when that includes this machine.
  bool pass_on(int from, int to, const nlohmann::json& message);
  void handle_as_guest(const nlohmann::json& message, std::vector<NetworkEvent>& events);
  // What a machine still connecting hears: that the game is deciding on it, that it was turned
  // away, or the welcome.
  void hear_while_joining(const nlohmann::json& message, std::vector<NetworkEvent>& events);
  // What a machine still connecting waits for: the host's welcome, with its id and who else is here.
  void accept_welcome(const nlohmann::json& message, std::vector<NetworkEvent>& events);
  // What a machine in the game hears from its host: who came, who went, what the others said.
  void hear_of_the_game(const nlohmann::json& message, std::vector<NetworkEvent>& events);
  // Whether a welcome can be believed: an id this machine can have, which the list of the others lacks.
  bool believable_welcome(int given, const std::vector<int>& others) const;
  // Whether a host's word that `peer` arrived can be believed: nobody arrives twice, as this
  // machine, or as nobody.
  bool can_arrive(int peer) const { return !knows(peer) && peer != id_ && peer >= kHostId; }
  void hear_game_message(const nlohmann::json& message, std::vector<NetworkEvent>& events);
  void peer_arrived(int peer, std::vector<NetworkEvent>& events);
  void peer_left(int peer, std::vector<NetworkEvent>& events);
  void drop_closed_links(std::vector<NetworkEvent>& events);
  // The host's part of it: the others hear who left.
  void farewell_closed_links(std::vector<NetworkEvent>& events);
  void end_game_if_host_gone(std::vector<NetworkEvent>& events);
  // Whether the link to the host closed, and the game with it.
  bool host_gone() const;
  // Forgets the links that closed; whose they were, in the order they were held (0: never let in).
  std::vector<int> take_closed_links();
  // Whether a message may go out: from a machine in a game, to someone else, within what a message may be.
  bool can_send(const std::string& name, const nlohmann::json& data, int to) const;
  // Whether a guest may address `to`: everyone, the host, or someone it knows of.
  bool can_reach(int to) const { return reaches_host(to) || knows(to); }
  bool send_through_host(const nlohmann::json& message, int to);
  bool send_as_host(const nlohmann::json& message, int to);
  void queue_for_everyone_but(int peer, const nlohmann::json& message);
  Link* link_to(int peer);
  void go_offline();

  State state_ = State::Offline;
  int id_ = 0;
  std::uint16_t port_ = 0;
  float connecting_for_ = 0.0f;
  bool through_relay_ = false;  // the game is a room on a relay
  bool opening_room_ = false;   // ... which this machine asked to open
  bool server_hosted_ = false;  // the game's host is a dedicated server
  bool approving_ = false;      // the game decides who comes in (require_approval)
  int last_ticket_ = 0;
  std::string refusal_;  // why the host turned this machine away, while joining
  std::string room_;
  Socket listener_;
  std::vector<Link> links_;
  std::vector<int> peers_;
  std::vector<NetworkEvent> pending_events_;  // what happened between polls, for the next one
};

}  // namespace redgem::net
