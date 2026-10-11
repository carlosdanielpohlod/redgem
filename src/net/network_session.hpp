#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "net/frame_buffer.hpp"
#include "net/socket.hpp"

namespace redgem::net {

// Who a message is for, besides one peer's id.
inline constexpr int kEveryone = 0;
// The id of the machine hosting the game. Whoever joins gets 2, 3, ... in the order they arrive.
inline constexpr int kHostId = 1;
// Machines in one game, the host included.
inline constexpr int kMaxPeers = 16;
// What a message may be: how long its name and the keys of its hashes, and how deep its data may
// nest (arrays in hashes in arrays...). Checked where a message is sent and again where it arrives,
// since what arrives may come from a program that is not this one.
inline constexpr std::size_t kMaxNameBytes = 64;
inline constexpr int kMaxDataDepth = 16;

// Whether `data` is something a message may carry.
bool acceptable_data(const nlohmann::json& data);

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
class NetworkSession {
 public:
  enum class State { Offline, Connecting, Online };

  // Starts a game others can join on `port` (0 picks a free one, see port()).
  bool host(std::uint16_t port, std::string& error);
  // Starts joining the game hosted at `address`. A Connected or Failed event tells how it went.
  bool join(const std::string& address, std::uint16_t port, std::string& error);
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
    Socket socket;
    FrameBuffer incoming;
    std::vector<std::uint8_t> outgoing;
    int peer = 0;            // 0 on the host until the newcomer said hello and was let in
    float waiting_for = 0.0f;  // seconds a newcomer has kept the host waiting for that hello
    bool closed = false;
  };

  void accept_newcomers();
  void admit(Link& link, std::vector<NetworkEvent>& events);
  void drop_silent_newcomers(float seconds);
  void await_connection(float seconds, std::vector<NetworkEvent>& events);
  // Reads and writes what the link can right now; marks it closed when the other side is gone.
  void pump(Link& link);
  void handle(Link& link, const std::string& text, std::vector<NetworkEvent>& events);
  void handle_as_host(Link& link, const nlohmann::json& message, std::vector<NetworkEvent>& events);
  void handle_as_guest(const nlohmann::json& message, std::vector<NetworkEvent>& events);
  void drop_closed_links(std::vector<NetworkEvent>& events);
  void queue(Link& link, const nlohmann::json& message);
  void queue_for_everyone_but(int peer, const nlohmann::json& message);
  Link* link_to(int peer);
  void go_offline();

  State state_ = State::Offline;
  int id_ = 0;
  int next_id_ = kHostId + 1;
  std::uint16_t port_ = 0;
  float connecting_for_ = 0.0f;
  bool announce_hosting_ = false;
  Socket listener_;
  std::vector<Link> links_;
  std::vector<int> peers_;
};

}  // namespace redgem::net
