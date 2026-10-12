#include "net/socket.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <thread>
#include <utility>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace redgem::net {

namespace {

constexpr int kWaitingConnections = 16;  // connections the system holds until accept() takes them
constexpr std::size_t kReadChunkBytes = 16 * 1024;
constexpr int kEnabled = 1;
constexpr std::size_t kDatagramBytes = 2048;  // more than any message games on a network exchange to find each other
constexpr std::intptr_t kInvalidSocket = -1;
constexpr float kMillisecondsPerSecond = 1000.0f;
constexpr float kLongestWaitSeconds = 3600.0f;  // a wait asked to last longer is cut to this

#ifdef _WIN32
using NativeSocket = SOCKET;
using Length = int;
constexpr int kSendFlags = 0;
constexpr WORD kWinsockVersion = MAKEWORD(2, 2);

// Winsock has to be started once before any socket call.
void start_sockets() {
  static const bool started = [] {
    WSADATA data;
    return WSAStartup(kWinsockVersion, &data) == 0;
  }();
  static_cast<void>(started);
}

bool would_block() {
  const int error = WSAGetLastError();
  return error == WSAEWOULDBLOCK || error == WSAEINPROGRESS;
}

void close_native(NativeSocket socket) { closesocket(socket); }

bool make_non_blocking(NativeSocket socket) {
  u_long enabled = kEnabled;
  return ioctlsocket(socket, FIONBIO, &enabled) == 0;
}

// Whether a connection in the making has an answer yet: 1 yes, 0 not yet, negative on an error.
int connection_answered(NativeSocket socket) {
  fd_set writable;
  fd_set failed;
  FD_ZERO(&writable);
  FD_ZERO(&failed);
  FD_SET(socket, &writable);
  FD_SET(socket, &failed);
  timeval no_wait{};
  return select(0, nullptr, &writable, &failed, &no_wait);  // (Windows ignores the first argument)
}

std::string last_error() { return "socket error " + std::to_string(WSAGetLastError()); }

using Waiting = WSAPOLLFD;
// (Windows refuses a wait that also asks for urgent data, which POLLIN would: only the usual kind.)
constexpr short kArrivals = POLLRDNORM;
constexpr short kRoomToSend = POLLWRNORM;

// Sleeps until one of `waiting` has what it waits for, `milliseconds` at most. How many have;
// negative when the system refused.
int wait_native(std::vector<Waiting>& waiting, int milliseconds) {
  return WSAPoll(waiting.data(), static_cast<ULONG>(waiting.size()), milliseconds);
}
#else
using NativeSocket = int;
using Length = socklen_t;
constexpr int kSendFlags = MSG_NOSIGNAL;  // a closed connection is an error to handle, not a signal

void start_sockets() {}

// (An interrupted call did nothing wrong either: it is tried again next frame.)
bool would_block() { return errno == EWOULDBLOCK || errno == EAGAIN || errno == EINPROGRESS || errno == EINTR; }

void close_native(NativeSocket socket) { ::close(socket); }

bool make_non_blocking(NativeSocket socket) {
  const int flags = fcntl(socket, F_GETFL, 0);
  return flags >= 0 && fcntl(socket, F_SETFL, flags | O_NONBLOCK) == 0;
}

std::string last_error() { return std::strerror(errno); }

// Whether a connection in the making has an answer yet: 1 yes, 0 not yet, negative on an error.
int connection_answered(NativeSocket socket) {
  pollfd waiting{socket, POLLOUT, 0};
  return poll(&waiting, 1, 0);
}

using Waiting = pollfd;
constexpr short kArrivals = POLLIN;
constexpr short kRoomToSend = POLLOUT;

// Sleeps until one of `waiting` has what it waits for, `milliseconds` at most. How many have;
// negative when the system refused (or a signal cut the sleep short).
int wait_native(std::vector<Waiting>& waiting, int milliseconds) {
  return poll(waiting.data(), static_cast<nfds_t>(waiting.size()), milliseconds);
}
#endif

// A wait's length as the system takes it: whole milliseconds, rounded up so that a short wait
// still sleeps, and never negative (which the system reads as "for ever").
int wait_milliseconds(float seconds) {
  if (!(seconds > 0.0f)) return 0;
  return static_cast<int>(std::ceil(std::min(seconds, kLongestWaitSeconds) * kMillisecondsPerSecond));
}

NativeSocket native(std::intptr_t handle) { return static_cast<NativeSocket>(handle); }

void set_option(NativeSocket socket, int level, int option) {
  setsockopt(socket, level, option, reinterpret_cast<const char*>(&kEnabled), sizeof(kEnabled));
}

// A socket of the given kind that never waits; kInvalidSocket, with `error` set, when there is none to have.
std::intptr_t open_non_blocking(int type, int protocol, std::string& error) {
  start_sockets();
  const NativeSocket socket = ::socket(AF_INET, type, protocol);
  if (static_cast<std::intptr_t>(socket) < 0 || !make_non_blocking(socket)) {
    error = last_error();
    if (static_cast<std::intptr_t>(socket) >= 0) close_native(socket);
    return kInvalidSocket;
  }
  return static_cast<std::intptr_t>(socket);
}

// A TCP connection sends small messages right away, and notices in time a machine that vanished.
void set_stream_options(NativeSocket socket) {
  set_option(socket, IPPROTO_TCP, TCP_NODELAY);
  set_option(socket, SOL_SOCKET, SO_KEEPALIVE);
}

// A TCP socket that never waits and sends small messages right away.
std::intptr_t open_stream(std::string& error) {
  const std::intptr_t handle = open_non_blocking(SOCK_STREAM, IPPROTO_TCP, error);
  if (handle != kInvalidSocket) set_stream_options(native(handle));
  return handle;
}

// A UDP socket that never waits and may talk to every machine on the local network.
std::intptr_t open_datagrams(std::string& error) {
  const std::intptr_t handle = open_non_blocking(SOCK_DGRAM, IPPROTO_UDP, error);
  if (handle == kInvalidSocket) return handle;
  set_option(native(handle), SOL_SOCKET, SO_BROADCAST);
  // Two games on one machine both answer searches: they share the port.
  set_option(native(handle), SOL_SOCKET, SO_REUSEADDR);
#ifdef SO_REUSEPORT
  set_option(native(handle), SOL_SOCKET, SO_REUSEPORT);
#endif
  return handle;
}

// Takes a port of this machine, on every address it has.
bool bind_to_port(NativeSocket socket, std::uint16_t port) {
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(port);
  return bind(socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0;
}

// The port a socket got; 0 when it has none.
std::uint16_t bound_port(NativeSocket socket) {
  sockaddr_in address{};
  Length size = sizeof(address);
  if (getsockname(socket, reinterpret_cast<sockaddr*>(&address), &size) != 0) return 0;
  return ntohs(address.sin_port);
}

std::string in_numbers(const sockaddr_in& address) {
  char text[INET_ADDRSTRLEN] = {};
  inet_ntop(AF_INET, &address.sin_addr, text, sizeof(text));
  return text;
}

// Where a machine is, by its name or its address; nullptr when nothing is found. An address in
// numbers needs no looking up; only a name asks the network, and waits for it.
addrinfo* look_up(const std::string& address, std::uint16_t port) {
  addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_NUMERICHOST;
  const std::string service = std::to_string(port);
  addrinfo* found = nullptr;
  if (getaddrinfo(address.c_str(), service.c_str(), &hints, &found) == 0) return found;
  hints.ai_flags = 0;
  found = nullptr;
  if (getaddrinfo(address.c_str(), service.c_str(), &hints, &found) != 0) return nullptr;
  return found;
}

// Starts connecting; false when the system refused outright (a connection still in the making is fine).
bool start_connecting(NativeSocket socket, const addrinfo& to) {
  const int result = connect(socket, to.ai_addr, static_cast<Length>(to.ai_addrlen));
  return result == 0 || would_block();
}

// Appends what one call takes from a connection to `into`. How many bytes that was: 0 when the
// other side hung up, negative when nothing came (see would_block).
std::ptrdiff_t read_chunk(NativeSocket socket, std::vector<std::uint8_t>& into) {
  char chunk[kReadChunkBytes];
  const auto count = recv(socket, chunk, sizeof(chunk), 0);
  if (count > 0) into.insert(into.end(), chunk, chunk + count);
  return static_cast<std::ptrdiff_t>(count);
}

IoStatus read_so_far(std::size_t got) { return got > 0 ? IoStatus::Ok : IoStatus::WouldBlock; }

// What a call that took nothing means: nothing more waiting right now, or a connection gone wrong.
IoStatus nothing_more(std::size_t got) { return would_block() ? read_so_far(got) : IoStatus::Closed; }

}  // namespace

Socket::Socket(Socket&& other) noexcept : handle_(std::exchange(other.handle_, kNoHandle)) {}

Socket& Socket::operator=(Socket&& other) noexcept {
  if (this != &other) {
    close();
    handle_ = std::exchange(other.handle_, kNoHandle);
  }
  return *this;
}

void Socket::close() {
  if (handle_ == kNoHandle) return;
  close_native(native(handle_));
  handle_ = kNoHandle;
}

Socket Socket::listen_on(std::uint16_t port, std::string& error) {
  Socket listener(open_stream(error));
  if (!listener.valid()) return listener;
#ifndef _WIN32
  // Lets a game host again right after it stopped (on Windows this option means something else).
  set_option(native(listener.handle_), SOL_SOCKET, SO_REUSEADDR);
#endif
  if (!bind_to_port(native(listener.handle_), port) || listen(native(listener.handle_), kWaitingConnections) != 0) {
    error = last_error();
    listener.close();
  }
  return listener;
}

Socket Socket::connect_to(const std::string& address, std::uint16_t port, std::string& error) {
  start_sockets();
  addrinfo* found = look_up(address, port);
  if (found == nullptr) {
    error = "cannot find " + address;
    return Socket();
  }
  Socket stream(open_stream(error));
  if (stream.valid() && !start_connecting(native(stream.handle_), *found)) {
    error = last_error();
    stream.close();
  }
  freeaddrinfo(found);
  return stream;
}

IoStatus Socket::accept(Socket& guest) {
  if (!valid()) return IoStatus::Closed;
  const NativeSocket taken = ::accept(native(handle_), nullptr, nullptr);
  if (static_cast<std::intptr_t>(taken) < 0) return would_block() ? IoStatus::WouldBlock : IoStatus::Closed;
  guest = Socket(static_cast<std::intptr_t>(taken));
  if (!make_non_blocking(taken)) {
    guest.close();
    return IoStatus::Closed;
  }
  set_stream_options(taken);
  return IoStatus::Ok;
}

std::string Socket::peer_address() const {
  if (!valid()) return {};
  sockaddr_in address{};
  Length size = sizeof(address);
  if (getpeername(native(handle_), reinterpret_cast<sockaddr*>(&address), &size) != 0) return {};
  return in_numbers(address);
}

IoStatus Socket::connect_status() {
  if (!valid()) return IoStatus::Closed;
  // A connection in the making becomes writable when it succeeds, and reports an error when not.
  const int answered = connection_answered(native(handle_));
  if (answered < 0) return would_block() ? IoStatus::WouldBlock : IoStatus::Closed;
  if (answered == 0) return IoStatus::WouldBlock;
  int problem = 0;
  Length size = sizeof(problem);
  if (getsockopt(native(handle_), SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&problem), &size) != 0 ||
      problem != 0) {
    return IoStatus::Closed;
  }
  return IoStatus::Ok;
}

IoStatus Socket::read(std::vector<std::uint8_t>& into, std::size_t limit) {
  if (!valid()) return IoStatus::Closed;
  std::size_t got = 0;
  while (got < limit) {
    const std::ptrdiff_t count = read_chunk(native(handle_), into);
    if (count == 0) return IoStatus::Closed;  // hung up
    if (count < 0) return nothing_more(got);
    got += static_cast<std::size_t>(count);
  }
  return read_so_far(got);
}

IoStatus Socket::write(std::vector<std::uint8_t>& bytes) {
  if (!valid()) return IoStatus::Closed;
  while (!bytes.empty()) {
    const auto sent =
        send(native(handle_), reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()), kSendFlags);
    if (sent > 0) {
      bytes.erase(bytes.begin(), bytes.begin() + sent);
      continue;
    }
    return would_block() ? IoStatus::WouldBlock : IoStatus::Closed;
  }
  return IoStatus::Ok;
}

std::uint16_t Socket::local_port() const { return valid() ? bound_port(native(handle_)) : 0; }

bool Socket::wait(const std::vector<Watched>& sockets, float seconds) {
  std::vector<Waiting> waiting;
  for (const Watched& watched : sockets) {
    if (watched.socket == nullptr || !watched.socket->valid()) continue;
    const short events = watched.sending ? static_cast<short>(kArrivals | kRoomToSend) : kArrivals;
    waiting.push_back({native(watched.socket->handle_), events, 0});
  }
  const int milliseconds = wait_milliseconds(seconds);
  if (waiting.empty()) {
    // With nothing to watch only the time can end the wait (and Windows refuses an empty list).
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    return false;
  }
  return wait_native(waiting, milliseconds) > 0;
}

// --- Datagrams -----------------------------------------------------------------------------

Datagrams::Datagrams(Datagrams&& other) noexcept : handle_(std::exchange(other.handle_, kNoHandle)) {}

Datagrams& Datagrams::operator=(Datagrams&& other) noexcept {
  if (this != &other) {
    close();
    handle_ = std::exchange(other.handle_, kNoHandle);
  }
  return *this;
}

void Datagrams::close() {
  if (handle_ == kNoHandle) return;
  close_native(native(handle_));
  handle_ = kNoHandle;
}

Datagrams Datagrams::open_on(std::uint16_t port, std::string& error) {
  Datagrams datagrams(open_datagrams(error));
  if (datagrams.valid() && !bind_to_port(native(datagrams.handle_), port)) {
    error = last_error();
    datagrams.close();
  }
  return datagrams;
}

bool Datagrams::send_to(const std::string& address, std::uint16_t port, const std::string& message) {
  if (!valid()) return false;
  sockaddr_in to{};
  to.sin_family = AF_INET;
  to.sin_port = htons(port);
  if (inet_pton(AF_INET, address.c_str(), &to.sin_addr) != 1) return false;
  const auto sent = sendto(native(handle_), message.data(), static_cast<int>(message.size()), kSendFlags,
                           reinterpret_cast<const sockaddr*>(&to), sizeof(to));
  return sent == static_cast<decltype(sent)>(message.size());
}

bool Datagrams::receive(std::string& message, std::string& from_address, std::uint16_t& from_port) {
  if (!valid()) return false;
  char chunk[kDatagramBytes];
  sockaddr_in from{};
  Length size = sizeof(from);
  const auto count = recvfrom(native(handle_), chunk, sizeof(chunk), 0, reinterpret_cast<sockaddr*>(&from), &size);
  if (count <= 0) return false;
  message.assign(chunk, static_cast<std::size_t>(count));
  from_address = in_numbers(from);
  from_port = ntohs(from.sin_port);
  return true;
}

std::uint16_t Datagrams::local_port() const { return valid() ? bound_port(native(handle_)) : 0; }

}  // namespace redgem::net
