#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "net/stun/datagram_link.hpp"

namespace redgem::net::stun {

// The most datagrams a responder reads in one poll: a flood waits for the next one rather than
// holding up the rest of the program (the relay's rooms).
inline constexpr std::size_t kMostReadsPerPoll = 64;

// A small STUN server: answers each Binding request that arrives on `link` with the address and
// port it came from. A relay runs one on its own port number (over UDP), so games that meet there
// can learn their public address from a machine they already trust, with no third party. It keeps
// nothing between requests, and its answer is barely longer than the request (32 bytes for 20):
// little use to whoever would bounce traffic off it.
class StunResponder {
 public:
  explicit StunResponder(DatagramLink& link) : link_(link) {}

  // Answers what arrived since the last poll; how many requests were answered.
  std::size_t poll();

 private:
  // Answers one arrival when it is a Binding request; whether it was.
  bool answer(const Datagram& arrival);

  DatagramLink& link_;
};

// A StunResponder on a UDP port of this machine: what redgem-relay runs beside its rooms.
class UdpStunServer {
 public:
  UdpStunServer() = default;
  UdpStunServer(const UdpStunServer&) = delete;
  UdpStunServer& operator=(const UdpStunServer&) = delete;
  UdpStunServer(UdpStunServer&&) = delete;
  UdpStunServer& operator=(UdpStunServer&&) = delete;
  ~UdpStunServer() = default;

  // Starts answering on `port` (0 for any free one); false, with `error`, when it cannot be had.
  bool open(std::uint16_t port, std::string& error);
  bool serving() const { return responder_ != nullptr; }
  std::uint16_t port() const { return datagrams_.local_port(); }
  // Answers what arrived; nothing when it is not serving.
  std::size_t poll() { return serving() ? responder_->poll() : 0; }

 private:
  Datagrams datagrams_;
  std::unique_ptr<UdpLink> link_;
  std::unique_ptr<StunResponder> responder_;
};

}  // namespace redgem::net::stun
