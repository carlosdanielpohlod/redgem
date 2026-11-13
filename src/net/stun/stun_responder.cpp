// A STUN server that answers Binding requests and nothing else.

#include "net/stun/stun_responder.hpp"

#include "net/stun/stun_message.hpp"

namespace redgem::net::stun {

std::size_t StunResponder::poll() {
  std::size_t answered = 0;
  for (std::size_t read = 0; read < kMostReadsPerPoll; ++read) {
    const auto arrival = link_.receive();
    if (!arrival) break;
    if (answer(*arrival)) ++answered;
  }
  return answered;
}

bool StunResponder::answer(const Datagram& arrival) {
  TransactionId id{};
  if (!is_binding_request(arrival.bytes, id)) return false;
  const std::string reply = binding_success(id, MappedAddress{arrival.address, arrival.port});
  if (reply.empty()) return false;
  link_.send(Datagram{reply, arrival.address, arrival.port});
  return true;
}

bool UdpStunServer::open(std::uint16_t port, std::string& error) {
  datagrams_ = Datagrams::open_on(port, error);
  if (!datagrams_.valid()) return false;
  link_ = std::make_unique<UdpLink>(datagrams_);
  responder_ = std::make_unique<StunResponder>(*link_);
  return true;
}

}  // namespace redgem::net::stun
