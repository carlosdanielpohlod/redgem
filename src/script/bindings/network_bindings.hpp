#pragma once

// What the files of the Network natives share: the session and the discovery they work on, and
// the ports they are handed.

#include <mruby.h>

#include <cstdint>

namespace redgem {

namespace net {
class NetworkSession;
class LanDiscovery;
}  // namespace net

// A RuntimeError for a script run outside a game, which has neither.
net::NetworkSession& network(mrb_state* mrb);
net::LanDiscovery& discovery(mrb_state* mrb);

// A port as the network takes it; an ArgumentError for a number that is not one.
std::uint16_t port_arg(mrb_state* mrb, mrb_int port);

// Network.search and what it finds (network_search_bindings.cpp).
void install_network_search_bindings(RClass* network_module, mrb_state* mrb);

}  // namespace redgem
