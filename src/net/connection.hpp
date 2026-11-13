#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "net/frame_buffer.hpp"
#include "net/secure/fingerprint.hpp"
#include "net/secure/tls_identity.hpp"
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
// How long a machine that connected has to finish its TLS handshake, of the time it has to say
// what it wants (kJoinTimeoutSeconds): one that keeps a handshake hanging holds a place, so it
// is let go sooner. (Nothing waits for it meanwhile: every handshake goes on as bytes arrive.)
inline constexpr float kHandshakeTimeoutSeconds = 3.0f;

class TlsSession;

// One machine's end of a TCP connection that carries messages (see wire.hpp): a host's to each
// machine that joined, a joiner's to its host, a relay's to each machine it serves. What they all
// need is here once: what arrives is cut into messages and checked, what waits is sent, a machine
// that went silent is noticed, and no machine costs the others more than its share.
// Nothing here waits: pump() it every poll.
// It is encrypted (TLS, see net/secure/tls_session.hpp) when made so: no message moves before the
// handshake is over, nor, on the side that calls, before the other proved to be the machine its
// TlsTrust expects. Games and the server programs always encrypt; plaintext is for the engine's
// own tests of the protocol, which write it byte by byte.
class Connection {
 public:
  Connection();
  ~Connection();
  Connection(Connection&& other) noexcept;
  Connection& operator=(Connection&& other) noexcept;
  // Over the socket of a machine that connected to this one. `pending_limit` is how much may wait
  // to be sent to it before it counts as no longer listening. With an `identity` the connection
  // is encrypted, this machine proving itself with it; without, it is plaintext.
  static Connection accepted(Socket socket, std::size_t pending_limit = kMaxPendingBytes,
                             const std::shared_ptr<const TlsIdentity>& identity = nullptr);
  // Over a socket that is still connecting out: what is sent meanwhile goes once it is through
  // (encrypted, and the other side checked, as `trust` says).
  static Connection calling(Socket socket, const TlsTrust& trust = TlsTrust::plaintext());

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
  // Time passing for a newcomer that has not said what it wants yet: it is given up on in the end
  // (sooner while its handshake is not over: kHandshakeTimeoutSeconds).
  void keep_waiting(float seconds);

  // Lets go of the other side now: nothing more of it is read, handled or sent.
  void close();
  // Turns the other side away, telling it why first (see wire::refusal): its last word goes out
  // before the connection is let go.
  void refuse(const std::string& reason);
  // Whether nothing more comes of it: it was let go, or the other side hung up and everything it
  // had said before that was taken.
  bool closed() const { return dropped_ || (gone_ && !incoming_.message_waiting()); }
  // Whether it closed without either side meaning to: the other side vanished (the network
  // failed, it went silent for too long, or stopped taking what was sent to it), and was not let
  // go for anything else nor said goodbye. What reconnecting is for.
  bool lost() const { return lost_ || (gone_ && !dropped_); }
  // Whether next_message has something to give without anything more arriving.
  bool message_waiting() const { return incoming_.message_waiting(); }
  // Whether the next pump and next_message have something to do without anything more arriving:
  // a message, or what arrived and waits in the TLS session.
  bool work_waiting() const;
  // Whether it is encrypted, and its handshake is not over yet.
  bool handshaking() const;
  // Why its TLS failed (a handshake that did not work out, another side than the one expected);
  // empty when it did not, or it has none.
  std::string failure() const;
  // The bytes held for it: what arrived and was not taken yet, and what waits to be sent.
  std::size_t held() const { return incoming_.buffered() + outgoing_.size(); }
  // What a wait watches its socket for.
  Watched watched() const { return {&socket_, has_to_send()}; }
  // Whether something waits for the socket to take it: messages, or the TLS handshake's bytes.
  bool has_to_send() const;
  // Where the other side connected from, in numbers.
  const std::string& address() const { return address_; }

 private:
  Connection(Socket socket, std::size_t pending_limit, bool established);
  // Whether the connection is made (a call out takes a moment; a refused one counts as hung up).
  bool established();
  // Encrypts it with `tls`; with none (TLS could not start), it is closed.
  void encrypt(std::unique_ptr<TlsSession> tls);
  // Whether messages can move: the connection is made, and its handshake is over.
  bool made();
  // Whether its handshake is over (or it has none); one that failed hangs up.
  bool secured();
  // How long a newcomer may keep quiet before it is let go.
  float patience() const { return handshaking() ? kHandshakeTimeoutSeconds : kJoinTimeoutSeconds; }
  std::size_t take_in();
  // Moves bytes through the socket, or through the TLS session over it.
  IoStatus read_some(std::vector<std::uint8_t>& into, std::size_t limit);
  IoStatus write_out();
  // Whether nothing more goes to the other side: it hung up, or was let go.
  bool hung_up() const { return gone_ || dropped_; }
  // Whether what waits can be written out now: to a side that is there, over a connection that is
  // made (and encrypted, when it is to be).
  bool can_write() const { return !hung_up() && established_ && !handshaking(); }
  // Whether one more message may be taken this poll (see kMaxMessagesPerPoll).
  bool takes_more() const { return !dropped_ && taken_this_poll_ < kMaxMessagesPerPoll; }
  // The other side is gone: nothing more is sent or read, but what it said is still to be taken.
  void hang_up();
  // What a message that arrived is to this side (see next_message).
  std::optional<nlohmann::json> understood(const std::optional<nlohmann::json>& message);
  // Whether a message is the first words of another version of the protocol.
  bool first_of_another_protocol(const nlohmann::json& message) const {
    return !heard_ && !wire::speaks_our_protocol(message);
  }
  std::optional<nlohmann::json> another_protocol(int theirs);
  // What is not a message of ours closes the connection: nothing of it is used.
  std::optional<nlohmann::json> not_ours();

  Socket socket_;
  std::unique_ptr<TlsSession> tls_;  // null: plaintext
  FrameBuffer incoming_;
  std::vector<std::uint8_t> outgoing_;
  std::string address_;
  std::string tls_error_;  // why TLS could not start
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
  bool lost_ = false;  // let go for its silence, or for not listening: gone without a goodbye
};

}  // namespace redgem::net
