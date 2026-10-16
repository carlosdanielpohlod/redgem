#include "net/socket_native.hpp"

#include <cstring>

#ifndef _WIN32
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace redgem::net::os {

namespace {

constexpr int kEnabled = 1;
constexpr std::size_t kDiscardChunkBytes = 16 * 1024;
// Read at most before a connection is closed anyway: a machine still sending at full speed is not
// waited out.
constexpr std::size_t kMostDiscardedBytes = 1024 * 1024;
#ifdef _WIN32
constexpr WORD kWinsockVersion = MAKEWORD(2, 2);
#endif

}  // namespace

#ifdef _WIN32
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

int wait_native(std::vector<Waiting>& waiting, int milliseconds) {
  return WSAPoll(waiting.data(), static_cast<ULONG>(waiting.size()), milliseconds);
}

void no_signal_on_closed(NativeSocket) {}  // (Windows sends no signals)
#else
void start_sockets() {}

// (An interrupted call did nothing wrong either: it is tried again next frame.)
bool would_block() { return errno == EWOULDBLOCK || errno == EAGAIN || errno == EINPROGRESS || errno == EINTR; }

void close_native(NativeSocket socket) { ::close(socket); }

bool make_non_blocking(NativeSocket socket) {
  const int flags = fcntl(socket, F_GETFL, 0);
  return flags >= 0 && fcntl(socket, F_SETFL, flags | O_NONBLOCK) == 0;
}

std::string last_error() { return std::strerror(errno); }

int connection_answered(NativeSocket socket) {
  pollfd waiting{socket, POLLOUT, 0};
  return poll(&waiting, 1, 0);
}

int wait_native(std::vector<Waiting>& waiting, int milliseconds) {
  return poll(waiting.data(), static_cast<nfds_t>(waiting.size()), milliseconds);
}

#ifdef SO_NOSIGPIPE
void no_signal_on_closed(NativeSocket socket) { set_option(socket, SOL_SOCKET, SO_NOSIGPIPE); }
#else
void no_signal_on_closed(NativeSocket) {}  // (every send says so itself: kSendFlags)
#endif
#endif

IoStatus later_or_closed() { return would_block() ? IoStatus::WouldBlock : IoStatus::Closed; }

void discard_unread(NativeSocket socket) {
  char chunk[kDiscardChunkBytes];
  for (std::size_t discarded = 0; discarded < kMostDiscardedBytes;) {
    const auto count = recv(socket, chunk, sizeof(chunk), 0);
    if (count <= 0) return;  // nothing more for now, or nothing more ever
    discarded += static_cast<std::size_t>(count);
  }
}

void set_option(NativeSocket socket, int level, int option) {
  setsockopt(socket, level, option, reinterpret_cast<const char*>(&kEnabled), sizeof(kEnabled));
}

std::intptr_t open_non_blocking(int type, int protocol, std::string& error) {
  start_sockets();
  const NativeSocket socket = ::socket(AF_INET, type, protocol);
  if (!is_socket(socket)) {
    error = last_error();
    return kInvalidSocket;
  }
  if (make_non_blocking(socket)) return static_cast<std::intptr_t>(socket);
  error = last_error();
  close_native(socket);
  return kInvalidSocket;
}

bool bind_to_port(NativeSocket socket, std::uint16_t port) {
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(port);
  return bind(socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0;
}

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

}  // namespace redgem::net::os
