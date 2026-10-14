// Datagrams: short messages over UDP, to one machine or to every machine nearby.

#include "net/socket.hpp"

#include <utility>

#include "net/socket_native.hpp"

namespace redgem::net {

namespace {

using namespace os;

constexpr std::size_t kDatagramBytes = 2048;  // more than any message games on a network exchange to find each other

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

// Whether a send took the whole of a message of `size` bytes.
template <typename Count>
bool went_whole(Count sent, std::size_t size) {
  return sent >= 0 && static_cast<std::size_t>(sent) == size;
}

}  // namespace

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
  if (!datagrams.valid()) return datagrams;
  if (bind_to_port(native(datagrams.handle_), port)) return datagrams;
  error = last_error();
  datagrams.close();
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
  return went_whole(sent, message.size());
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
