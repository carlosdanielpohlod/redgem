#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/clock.hpp"

namespace redgem::net {

// The longest a wait sleeps: one asked to last longer is cut to this.
inline constexpr float kLongestWaitSeconds = 3600.0f;

// A wait's length as the system takes it: whole milliseconds, rounded up so that a short wait
// still sleeps, never more than kLongestWaitSeconds, and 0 (no sleep) for a wait of no time or of
// a length that is no number (never negative, which the system reads as "for ever").
int wait_milliseconds(float seconds);

enum class IoStatus {
  Ok,
  WouldBlock,  // nothing to do right now: try again next frame
  Closed       // the other side is gone (or never answered)
};

class Socket;

// A socket a wait watches: always for what arrives on it, and, when `sending`, for room to send.
struct Watched {
  const Socket* socket = nullptr;
  bool sending = false;
};

// A TCP connection, or a port being listened on. A game calls these every frame, so nothing here
// waits, with two exceptions: connect_to looks a machine's name up first, which can take a moment
// (an address in numbers, like 192.168.0.12, never does), and wait is there to wait. The one
// place the operating systems' socket calls are used.
class Socket {
 public:
  Socket() = default;
  ~Socket() { close(); }
  Socket(Socket&& other) noexcept;
  Socket& operator=(Socket&& other) noexcept;
  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;

  // Listens on a port of this machine (0 lets the system pick a free one). Invalid, with `error`
  // set, when the port cannot be used.
  static Socket listen_on(std::uint16_t port, std::string& error);
  // A local tooling endpoint: binds only IPv4 loopback, never a network interface.
  static Socket listen_local(std::uint16_t port, std::string& error);
  // Listens on one address of this machine only, given in numbers (127.0.0.1 for this machine
  // alone, 0.0.0.0 for every address it has). Invalid, with `error` set, when it cannot.
  static Socket listen_at(const std::string& address, std::uint16_t port, std::string& error);
  // Starts connecting to a machine by name or address; connect_status says how it is going.
  static Socket connect_to(const std::string& address, std::uint16_t port, std::string& error);

  bool valid() const { return handle_ != kNoHandle; }
  // Takes someone who connected to a listening socket into `guest`: Ok. WouldBlock when nobody is
  // waiting; Closed when someone is and cannot be taken (the system has no connection left to
  // give): whoever it is keeps waiting, so asking again at once gets the same answer.
  IoStatus accept(Socket& guest);
  // Ok once connected, WouldBlock while still trying, Closed when it was refused.
  IoStatus connect_status();
  // Appends what has arrived to `into`, up to `limit` bytes a call (the rest waits for the next).
  IoStatus read(std::vector<std::uint8_t>& into, std::size_t limit);
  // Sends as much of `bytes` as goes out now and removes that from its front.
  IoStatus write(std::vector<std::uint8_t>& bytes);
  // The port a listening socket got.
  std::uint16_t local_port() const;
  // The address, in numbers, of the machine at the other end; empty when there is none.
  std::string peer_address() const;
  // Closes it, having read and dropped what arrived and was never read first (up to 1 MB), so
  // that the other side gets a goodbye and not a reset.
  void close();
  // The same, reading at most `most_unread` bytes first: for a program that must bound what one
  // connection costs it.
  void close_reading_at_most(std::size_t most_unread);
  // Closes it at once with a reset: nothing more is read, nor sent.
  void reset();
  // Ends its connection both ways without closing it, from any thread: what is read or sent from
  // then on finds it ended, and on POSIX systems a wait on it ends at once (Windows lets a wait
  // sleep on). Its owner still closes it.
  void hang_up() const;

  // Sleeps until something happens on one of `sockets` (bytes arrive, someone connects, the other
  // side hangs up, or one that is `sending` can send again), for `seconds` at most. True when a
  // socket ended the wait, false when the time did. For a program with nothing else to do
  // meanwhile (a relay): a game, which draws frames, never calls it. With no socket to watch it
  // sleeps on `clock` (only the time can end it then).
  static bool wait(const std::vector<Watched>& sockets, float seconds, Clock& clock = steady_clock());

 private:
  static constexpr std::intptr_t kNoHandle = -1;
  explicit Socket(std::intptr_t handle) : handle_(handle) {}

  std::intptr_t handle_ = kNoHandle;
};

// Short messages to whoever listens on a port, with no connection: how games on the same
// network find each other. Nothing here waits either.
class Datagrams {
 public:
  Datagrams() = default;
  ~Datagrams() { close(); }
  Datagrams(Datagrams&& other) noexcept;
  Datagrams& operator=(Datagrams&& other) noexcept;
  Datagrams(const Datagrams&) = delete;
  Datagrams& operator=(const Datagrams&) = delete;

  // Opens on a port of this machine (0 for any free one). Several programs may share a port.
  static Datagrams open_on(std::uint16_t port, std::string& error);
  // Opens on a port of this machine that nothing else may share, and that never sends to every
  // machine nearby: for a server facing the internet (a relay's STUN answers).
  static Datagrams open_alone(std::uint16_t port, std::string& error);

  bool valid() const { return handle_ != kNoHandle; }
  // Sends to one machine by its address in numbers, or to every machine on the local network
  // (kEveryoneNearby). False when it could not be sent.
  bool send_to(const std::string& address, std::uint16_t port, const std::string& message);
  // The next message waiting, with who sent it; false when there is none.
  bool receive(std::string& message, std::string& from_address, std::uint16_t& from_port);
  std::uint16_t local_port() const;
  void close();

 private:
  static constexpr std::intptr_t kNoHandle = -1;
  explicit Datagrams(std::intptr_t handle) : handle_(handle) {}
  // `datagrams` bound to `port`; closed, with `error` set, when the port cannot be had.
  static Datagrams bound(Datagrams datagrams, std::uint16_t port, std::string& error);

  std::intptr_t handle_ = kNoHandle;
};

// The address that reaches every machine on the local network.
inline constexpr const char* kEveryoneNearby = "255.255.255.255";
inline constexpr const char* kThisMachine = "127.0.0.1";

}  // namespace redgem::net
