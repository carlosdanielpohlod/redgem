// Direct routes to a host first, the relay when none of them connects.

#include "net/stun/route_choice.hpp"

#include <utility>

#include "net/stun/step_time.hpp"

namespace redgem::net::stun {

RouteChooser::RouteChooser(Dialer& dialer, std::vector<Route> direct, Route relay)
    : dialer_(dialer), routes_(std::move(direct)) {
  routes_.push_back(std::move(relay));
  try_route(0);
}

void RouteChooser::update(float seconds) {
  if (state_ != ChoiceState::Trying) return;
  trying_for_ += counted_step(seconds);
  const DialStatus status = dialer_.status();
  if (status == DialStatus::Connected) {
    state_ = ChoiceState::Connected;
    return;
  }
  if (gave_up_on_route(status)) move_on();
}

bool RouteChooser::gave_up_on_route(DialStatus status) const {
  return status == DialStatus::Refused || trying_for_ >= try_seconds();
}

void RouteChooser::move_on() {
  if (current_ + 1 == routes_.size()) {
    state_ = ChoiceState::Failed;
    return;
  }
  try_route(current_ + 1);
}

void RouteChooser::try_route(std::size_t index) {
  current_ = index;
  trying_for_ = 0.0f;
  dialer_.dial(routes_[current_]);
}

void SocketDialer::dial(const Route& route) {
  std::string error;
  socket_ = Socket::connect_to(route.address, route.port, error);
}

DialStatus SocketDialer::status() {
  if (!socket_.valid()) return DialStatus::Refused;
  switch (socket_.connect_status()) {
    case IoStatus::Ok:
      return DialStatus::Connected;
    case IoStatus::WouldBlock:
      return DialStatus::Trying;
    case IoStatus::Closed:
      break;
  }
  return DialStatus::Refused;
}

}  // namespace redgem::net::stun
