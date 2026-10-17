#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "net/frame_buffer.hpp"
#include "net/socket/socket.hpp"
#include "net/wire.hpp"
#include "net/wire_codec.hpp"

namespace redgem::net {

// What a program that serves many machines at once (a relay, a dedicated server) allows each:
// what may wait to be sent to one machine, two of the longest messages (it holds many machines'
// queues at once, so it is far stricter than a game is with its few), and the connections one
// address may hold unless the program is told otherwise (players in one house share an address:
// a whole party fits; a campus behind one address needs more), and at most.
inline constexpr std::size_t kMaxPendingPerServed = 2 * kMaxFrameBytes;
inline constexpr int kDefaultConnectionsPerAddress = 24;
inline constexpr int kMostConnectionsPerAddress = 1024;

// One machine's end of a TCP connection that carries messages (see wire.hpp): a host's to each
// machine that joined, a joiner's to its host, a relay's to each machine it serves. What they all
// need is here once: what arrives is cut into messages and checked, what waits is sent, a machine
// that went silent is noticed, and no machine costs the others more than its share.
// Nothing here waits: pump() it every poll.
class Connection {
 public:
  Connection() = default;
  // Over the socket of a machine that connected to this one. `pending_limit` is how much may wait
  // to be sent to it before it counts as no longer listening.
  static Connection accepted(Socket socket, std::size_t pending_limit = kMaxPendingBytes);
  // Over a socket that is still connecting out: what is sent meanwhile goes once it is through.
  static Connection calling(Socket socket);

  // A poll's first step: reads what arrived and sends what waits. How many bytes arrived.
  std::size_t pump();
  // The next message that arrived whole: at most kMaxMessagesPerPoll between two pumps, the rest
  // wait for the next. What is not a message of ours closes the connection. The first words of
  // another version of the protocol are looked at before anything else of them (that version's
  // messages may hold what this one's may not): a machine that connected to this one is refused,
  // told why and logged; to one that this one called, they come out as a refusal saying that
  // version, and nothing more of them.
  std::optional<nlohmann::json> next_message();
  // Queues a message, sent right away when the connection takes it; what is left goes out with
  // the next pumps. With more than the limit waiting the other side stopped listening: closed.
  void send(const nlohmann::json& message);
  // The same, for a message packed already (wire::packed): one that goes to many is packed once.
  void send_packed(const std::vector<std::uint8_t>& message);
  // One try at sending what waits (pump does it too; this is for a last word before leaving).
  void flush();

  // Time passing for a machine that is in: says "I am here" when nothing was said to it lately,
  // and gives up on one not heard from for too long.
  void keep_in_touch(float seconds);
  // Time passing for a newcomer that has not said what it wants yet: it is given up on in the end.
  void keep_waiting(float seconds);

  // Lets go of the other side now: nothing more of it is read, handled or sent.
  void close();
  // Turns the other side away, telling it why first (see wire::refusal): its last word goes out
  // before the connection is let go.
  void refuse(const std::string& reason);
  // Whether nothing more comes of it: it was let go, or the other side hung up and everything it
  // had said before that was taken.
  bool closed() const { return dropped_ || (gone_ && !incoming_.message_waiting()); }
  // Whether next_message has something to give without anything more arriving.
  bool message_waiting() const { return incoming_.message_waiting(); }
  // The bytes held for it: what arrived and was not taken yet, and what waits to be sent.
  std::size_t held() const { return incoming_.buffered() + outgoing_.size(); }
  // What a wait watches its socket for.
  Watched watched() const { return {&socket_, !outgoing_.empty()}; }
  // Where the other side connected from, in numbers.
  const std::string& address() const { return address_; }

 private:
  Connection(Socket socket, std::size_t pending_limit, bool established);
  // Whether the connection is made (a call out takes a moment; a refused one counts as hung up).
  bool established();
  std::size_t take_in();
  // Whether nothing more goes to the other side: it hung up, or was let go.
  bool hung_up() const { return gone_ || dropped_; }
  // Whether what waits can be written out now: to a side that is there, over a connection that is made.
  bool can_write() const { return !hung_up() && established_; }
  // Whether one more message may be taken this poll (see kMaxMessagesPerPoll).
  bool takes_more() const { return !dropped_ && taken_this_poll_ < kMaxMessagesPerPoll; }
  // The other side is gone: nothing more is sent or read, but what it said is still to be taken.
  void hang_up();
  // What a message that arrived is to this side (see next_message).
  std::optional<nlohmann::json> understood(const std::optional<nlohmann::json>& message);
  // Whether a message is the first words of another version of the protocol.
  bool first_of_another_protocol(const std::optional<nlohmann::json>& message) const {
    return !heard_ && message.has_value() && !wire::speaks_our_protocol(*message);
  }
  std::optional<nlohmann::json> another_protocol(int theirs);

  Socket socket_;
  FrameBuffer incoming_;
  std::vector<std::uint8_t> outgoing_;
  std::string address_;
  std::size_t pending_limit_ = kMaxPendingBytes;
  int taken_this_poll_ = 0;   // messages (see kMaxMessagesPerPoll)
  float waiting_for_ = 0.0f;  // seconds a newcomer has kept quiet
  float quiet_for_ = 0.0f;    // seconds since anything arrived
  float since_said_ = 0.0f;   // seconds since anything was sent
  bool established_ = false;
  bool answering_ = false;  // the other side connected to this one
  bool heard_ = false;      // its first message was taken
  bool gone_ = false;
  bool dropped_ = false;
};

}  // namespace redgem::net
