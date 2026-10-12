#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace redgem::net {

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
  // Starts connecting to a machine by name or address; connect_status says how it is going.
  static Socket connect_to(const std::string& address, std::uint16_t port, std::string& error);

  bool valid() const { return handle_ != kNoHandle; }
  // Someone who connected to a listening socket; invalid when nobody is waiting.
  Socket accept();
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
  void close();

  // Sleeps until something happens on one of `sockets` (bytes arrive, someone connects, the other
  // side hangs up, or one that is `sending` can send again), for `seconds` at most. True when a
  // socket ended the wait, false when the time did. For a program with nothing else to do
  // meanwhile (a relay): a game, which draws frames, never calls it.
  static bool wait(const std::vector<Watched>& sockets, float seconds);

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

  std::intptr_t handle_ = kNoHandle;
};

// The address that reaches every machine on the local network.
inline constexpr const char* kEveryoneNearby = "255.255.255.255";
inline constexpr const char* kThisMachine = "127.0.0.1";

}  // namespace redgem::net
