// A STUN server that answers Binding requests and nothing else.

#include "net/stun/stun_responder.hpp"

#include "net/private_address.hpp"
#include "net/stun/stun_message.hpp"

namespace redgem::net::stun {

namespace {

// Addresses no answer goes to, by their first number: "this network" (0.x), and multicast,
// reserved and broadcast (224.x and up). A request from one is forged, to aim the answer at many.
constexpr int kThisNetwork = 0;
constexpr int kFirstMulticast = 224;

bool is_single_machine(std::uint32_t address) {
  const int first = first_octet(address);
  return first != kThisNetwork && first < kFirstMulticast;
}

// Whether an answer may go where a request says it came from: one machine, on a real port.
bool answerable(const Datagram& arrival) {
  const auto address = ipv4_number(arrival.address);
  return arrival.port != 0 && address.has_value() && is_single_machine(*address);
}

}  // namespace

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
  if (!answerable(arrival)) return false;
  if (!is_binding_request(arrival.bytes, id)) return false;
  const std::string reply = binding_success(id, MappedAddress{arrival.address, arrival.port});
  if (reply.empty()) return false;
  link_.send(Datagram{reply, arrival.address, arrival.port});
  return true;
}

bool UdpStunServer::open(std::uint16_t port, std::string& error) {
  datagrams_ = Datagrams::open_alone(port, error);
  if (!datagrams_.valid()) return false;
  link_ = std::make_unique<UdpLink>(datagrams_);
  responder_ = std::make_unique<StunResponder>(*link_);
  return true;
}

}  // namespace redgem::net::stun
