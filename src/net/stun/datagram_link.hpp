#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "net/socket/socket.hpp"

namespace redgem::net::stun {

// One datagram, and who sent it (or where it goes).
struct Datagram {
  std::string bytes;
  std::string address;
  std::uint16_t port = 0;
};

// Where STUN's datagrams come and go: a UDP port of this machine in a game or a relay, a fake one
// in the tests. Nothing here waits.
class DatagramLink {
 public:
  DatagramLink() = default;
  DatagramLink(const DatagramLink&) = delete;
  DatagramLink& operator=(const DatagramLink&) = delete;
  virtual ~DatagramLink() = default;

  // False when it could not be sent.
  virtual bool send(const Datagram& datagram) = 0;
  // The next datagram waiting; nothing when there is none.
  virtual std::optional<Datagram> receive() = 0;

 protected:
  DatagramLink(DatagramLink&&) = default;
  DatagramLink& operator=(DatagramLink&&) = default;
};

// A link over a UDP port of this machine.
class UdpLink final : public DatagramLink {
 public:
  explicit UdpLink(Datagrams& datagrams) : datagrams_(datagrams) {}

  bool send(const Datagram& datagram) override {
    return datagrams_.send_to(datagram.address, datagram.port, datagram.bytes);
  }

  std::optional<Datagram> receive() override {
    Datagram arrived;
    if (!datagrams_.receive(arrived.bytes, arrived.address, arrived.port)) return std::nullopt;
    return arrived;
  }

 private:
  Datagrams& datagrams_;
};

}  // namespace redgem::net::stun
