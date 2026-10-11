#include "net/socket.hpp"

#include <cstring>
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

constexpr int kWaitingConnections = 16;   // connections the system holds until accept() takes them
constexpr std::size_t kReadChunkBytes = 16 * 1024;
constexpr int kEnabled = 1;
constexpr std::intptr_t kInvalidSocket = -1;

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
#endif

NativeSocket native(std::intptr_t handle) { return static_cast<NativeSocket>(handle); }

void set_option(NativeSocket socket, int level, int option) {
  setsockopt(socket, level, option, reinterpret_cast<const char*>(&kEnabled), sizeof(kEnabled));
}

// A TCP socket that never waits and sends small messages right away.
std::intptr_t open_stream(std::string& error) {
  start_sockets();
  const NativeSocket socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (static_cast<std::intptr_t>(socket) < 0 || !make_non_blocking(socket)) {
    error = last_error();
    if (static_cast<std::intptr_t>(socket) >= 0) close_native(socket);
    return kInvalidSocket;
  }
  set_option(socket, IPPROTO_TCP, TCP_NODELAY);
  set_option(socket, SOL_SOCKET, SO_KEEPALIVE);  // a machine that vanished is noticed, in time
  return static_cast<std::intptr_t>(socket);
}

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
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(port);
  if (bind(native(listener.handle_), reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0 ||
      listen(native(listener.handle_), kWaitingConnections) != 0) {
    error = last_error();
    listener.close();
  }
  return listener;
}

Socket Socket::connect_to(const std::string& address, std::uint16_t port, std::string& error) {
  start_sockets();
  addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  const std::string service = std::to_string(port);
  addrinfo* found = nullptr;
  // An address in numbers needs no looking up; only a name asks the network, and waits for it.
  hints.ai_flags = AI_NUMERICHOST;
  if (getaddrinfo(address.c_str(), service.c_str(), &hints, &found) != 0) {
    hints.ai_flags = 0;
    found = nullptr;
    if (getaddrinfo(address.c_str(), service.c_str(), &hints, &found) != 0) found = nullptr;
  }
  if (found == nullptr) {
    error = "cannot find " + address;
    return Socket();
  }
  Socket stream(open_stream(error));
  if (stream.valid()) {
    const int result = connect(native(stream.handle_), found->ai_addr, static_cast<Length>(found->ai_addrlen));
    if (result != 0 && !would_block()) {
      error = last_error();
      stream.close();
    }
  }
  freeaddrinfo(found);
  return stream;
}

Socket Socket::accept() {
  if (!valid()) return Socket();
  const NativeSocket guest = ::accept(native(handle_), nullptr, nullptr);
  if (static_cast<std::intptr_t>(guest) < 0) return Socket();
  if (!make_non_blocking(guest)) {
    close_native(guest);
    return Socket();
  }
  set_option(guest, IPPROTO_TCP, TCP_NODELAY);
  return Socket(static_cast<std::intptr_t>(guest));
}

IoStatus Socket::connect_status() {
  if (!valid()) return IoStatus::Closed;
  // A connection in the making becomes writable when it succeeds, and reports an error when not.
  const int answered = connection_answered(native(handle_));
  if (answered < 0) return would_block() ? IoStatus::WouldBlock : IoStatus::Closed;
  if (answered == 0) return IoStatus::WouldBlock;
  int problem = 0;
  Length size = sizeof(problem);
  if (getsockopt(native(handle_), SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&problem), &size) != 0 || problem != 0) {
    return IoStatus::Closed;
  }
  return IoStatus::Ok;
}

IoStatus Socket::read(std::vector<std::uint8_t>& into, std::size_t limit) {
  if (!valid()) return IoStatus::Closed;
  std::size_t got = 0;
  while (got < limit) {
    char chunk[kReadChunkBytes];
    const auto count = recv(native(handle_), chunk, sizeof(chunk), 0);
    if (count > 0) {
      into.insert(into.end(), chunk, chunk + count);
      got += static_cast<std::size_t>(count);
      continue;
    }
    if (count == 0) return IoStatus::Closed;  // hung up
    if (would_block()) break;
    return IoStatus::Closed;
  }
  return got > 0 ? IoStatus::Ok : IoStatus::WouldBlock;
}

IoStatus Socket::write(std::vector<std::uint8_t>& bytes) {
  if (!valid()) return IoStatus::Closed;
  while (!bytes.empty()) {
    const auto sent = send(native(handle_), reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()),
                           kSendFlags);
    if (sent > 0) {
      bytes.erase(bytes.begin(), bytes.begin() + sent);
      continue;
    }
    return would_block() ? IoStatus::WouldBlock : IoStatus::Closed;
  }
  return IoStatus::Ok;
}

std::uint16_t Socket::local_port() const {
  if (!valid()) return 0;
  sockaddr_in address{};
  Length size = sizeof(address);
  if (getsockname(native(handle_), reinterpret_cast<sockaddr*>(&address), &size) != 0) return 0;
  return ntohs(address.sin_port);
}

}  // namespace redgem::net
