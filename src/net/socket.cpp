// Socket: making TCP connections (listening, calling out, taking who called).

#include "net/socket.hpp"

#include <utility>

#include "net/socket_native.hpp"

namespace redgem::net {

namespace {

using namespace os;

constexpr int kWaitingConnections = 16;  // connections the system holds until accept() takes them

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

// Takes a port of this machine and starts holding for accept() whoever connects to it.
bool listens_on(NativeSocket socket, std::uint16_t port) {
  return bind_to_port(socket, port) && listen(socket, kWaitingConnections) == 0;
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

// A TCP socket on its way to `to`; kInvalidSocket, with `error` set, when it could not set out.
std::intptr_t open_connecting(const addrinfo& to, std::string& error) {
  const std::intptr_t handle = open_stream(error);
  if (handle == kInvalidSocket) return handle;
  if (start_connecting(native(handle), to)) return handle;
  error = last_error();
  close_native(native(handle));
  return kInvalidSocket;
}

// Whether a connection that got its answer was made: the answer may be that it was refused.
bool went_through(NativeSocket socket) {
  int problem = 0;
  Length size = sizeof(problem);
  const int asked = getsockopt(socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&problem), &size);
  return asked == 0 && problem == 0;
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
  if (listens_on(native(listener.handle_), port)) return listener;
  error = last_error();
  listener.close();
  return listener;
}

Socket Socket::connect_to(const std::string& address, std::uint16_t port, std::string& error) {
  start_sockets();
  addrinfo* found = look_up(address, port);
  if (found == nullptr) {
    error = "cannot find " + address;
    return Socket();
  }
  Socket stream(open_connecting(*found, error));
  freeaddrinfo(found);
  return stream;
}

IoStatus Socket::accept(Socket& guest) {
  if (!valid()) return IoStatus::Closed;
  const NativeSocket taken = ::accept(native(handle_), nullptr, nullptr);
  if (!is_socket(taken)) return later_or_closed();
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
  if (answered < 0) return later_or_closed();
  if (answered == 0) return IoStatus::WouldBlock;
  return went_through(native(handle_)) ? IoStatus::Ok : IoStatus::Closed;
}

std::uint16_t Socket::local_port() const { return valid() ? bound_port(native(handle_)) : 0; }

}  // namespace redgem::net
