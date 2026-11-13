// A STUN Binding request sent until it is answered or the time is up.

#include "net/stun/stun_probe.hpp"

#include <utility>

namespace redgem::net::stun {

StunProbe::StunProbe(DatagramLink& link, MappedAddress server, const TransactionId& id)
    : link_(link), server_(std::move(server)), id_(id), request_(binding_request(id)) {}

void StunProbe::update(float seconds) {
  if (state_ != ProbeState::Asking) return;
  listen();
  if (state_ == ProbeState::Asking) keep_time(seconds);
}

void StunProbe::keep_time(float seconds) {
  waited_ += seconds;
  if (waited_ >= kProbeGiveUpSeconds) {
    state_ = ProbeState::Failed;
    return;
  }
  since_asked_ += seconds;
  if (since_asked_ >= kProbeRetrySeconds) ask();
}

void StunProbe::ask() {
  since_asked_ = 0.0f;
  link_.send(Datagram{request_, server_.address, server_.port});
}

void StunProbe::listen() {
  for (std::size_t read = 0; read < kMostDatagramsPerUpdate; ++read) {
    const auto arrival = link_.receive();
    if (!arrival) return;
    if (take_answer(*arrival)) return;
  }
}

bool StunProbe::take_answer(const Datagram& arrival) {
  if (MappedAddress{arrival.address, arrival.port} != server_) return false;
  const auto seen_at = mapped_address(arrival.bytes, id_);
  if (!seen_at) return false;
  found_ = *seen_at;
  state_ = ProbeState::Found;
  return true;
}

}  // namespace redgem::net::stun
