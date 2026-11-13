#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "net/socket/socket.hpp"

namespace redgem::net::stun {

// How long a direct route is given to connect before the next one is tried: a router that drops
// what nobody asked for lets a connection hang rather than refuse it. And the relay's, which is
// longer: it is the last route there is.
inline constexpr float kDirectTrySeconds = 3.0f;
inline constexpr float kRelayTrySeconds = 10.0f;

enum class RouteKind {
  Direct,  // straight to the host: its address on the local network, or the public one STUN found
  Relay    // through the relay both machines reach
};

struct Route {
  RouteKind kind = RouteKind::Direct;
  std::string address;
  std::uint16_t port = 0;
};

enum class DialStatus { Trying, Connected, Refused };

// What makes the connections a RouteChooser tries: sockets in a game (SocketDialer), a script of
// answers in the tests.
class Dialer {
 public:
  Dialer() = default;
  Dialer(const Dialer&) = delete;
  Dialer& operator=(const Dialer&) = delete;
  virtual ~Dialer() = default;

  // Starts connecting to `route`, dropping the connection being made before, if any.
  virtual void dial(const Route& route) = 0;
  // How the last dial is going.
  virtual DialStatus status() = 0;

 protected:
  Dialer(Dialer&&) = default;
  Dialer& operator=(Dialer&&) = default;
};

enum class ChoiceState {
  Trying,     // a route is being tried: route()
  Connected,  // route() connected
  Failed      // no route did, the relay included
};

// Tries the direct routes to a host one after the other, each for kDirectTrySeconds at most, and
// falls back to the relay when none connects: the order a game reaches a host behind a router in,
// fastest first. Direct routes only work where the host's router lets connections in (on the same
// network, a forwarded port, a router that maps ports in a friendly way); the relay always does.
// Call update() every frame until it is no longer Trying.
class RouteChooser {
 public:
  // Starts dialing the first route at once (the relay, when there are no direct ones).
  RouteChooser(Dialer& dialer, std::vector<Route> direct, Route relay);

  void update(float seconds);
  ChoiceState state() const { return state_; }
  // The route being tried, or the one that connected (the relay, after a failure).
  const Route& route() const { return routes_[current_]; }
  // Whether the route being tried, or that connected, goes through the relay.
  bool through_relay() const { return route().kind == RouteKind::Relay; }

 private:
  void try_route(std::size_t index);
  // Gives up on the route being tried: the next one is tried, or the choice failed.
  void move_on();
  // Whether the route being tried is to be given up on: refused, or out of time.
  bool gave_up_on_route(DialStatus status) const;
  float try_seconds() const { return through_relay() ? kRelayTrySeconds : kDirectTrySeconds; }

  Dialer& dialer_;
  std::vector<Route> routes_;  // the direct ones in order, the relay last
  std::size_t current_ = 0;
  float trying_for_ = 0.0f;
  ChoiceState state_ = ChoiceState::Trying;
};

// A Dialer of TCP connections. Dialing a machine by name looks the name up first, which can take
// a moment (see Socket::connect_to); routes in numbers never wait.
class SocketDialer final : public Dialer {
 public:
  void dial(const Route& route) override;
  DialStatus status() override;
  // The connection, once status() said Connected; the dialer holds none after.
  Socket take() { return std::move(socket_); }

 private:
  Socket socket_;
};

}  // namespace redgem::net::stun
