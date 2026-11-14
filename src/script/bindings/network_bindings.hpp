#pragma once

// What the files of the Network natives share: the session and the discovery they work on, and
// the ports they are handed.

#include <mruby.h>

#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

namespace redgem {

namespace net {
class NetworkSession;
class LanDiscovery;
class TlsTrust;
}  // namespace net

// A RuntimeError for a script run outside a game, which has neither.
net::NetworkSession& network(mrb_state* mrb);
net::LanDiscovery& discovery(mrb_state* mrb);

// A port as the network takes it; an ArgumentError for a number that is not one.
std::uint16_t port_arg(mrb_state* mrb, mrb_int port);

// What a join carries for the game joined to decide on (auth:): null for nil; an ArgumentError for
// anything but a hash within the limits (network_approval_bindings.cpp).
nlohmann::json auth_arg(mrb_state* mrb, mrb_value auth);
// How a join checks the machine that answers it (network_security_bindings.cpp): against
// `fingerprint` (a whole fingerprint or a host's code; null when the script gave none: then
// against game.json's "servers" for that address and port, or not at all). Every join is
// encrypted. An ArgumentError for a fingerprint that is neither.
net::TlsTrust trust_arg(mrb_state* mrb, const char* fingerprint, const std::string& address, std::uint16_t port);
// What a game hosted here proves itself with: Network.fingerprint, Network.host_code.
void install_network_security_bindings(RClass* network_module, mrb_state* mrb);
// Connection approval: Network.__approval, Network.__decide (network_approval_bindings.cpp).
void install_network_approval_bindings(RClass* network_module, mrb_state* mrb);
// Network.search and what it finds (network_search_bindings.cpp).
void install_network_search_bindings(RClass* network_module, mrb_state* mrb);
// Where the objects this machine moves are: Network.__pose (network_pose_bindings.cpp).
void install_network_pose_bindings(RClass* network_module, mrb_state* mrb);
// Movement validation: Network.__validate_movement, movement_rules, movement_violations
// (network_movement_bindings.cpp).
void install_network_movement_bindings(RClass* network_module, mrb_state* mrb);
// What the session is like right now: online?, host?, server?, id, peers... (network_state_bindings.cpp).
void install_network_state_bindings(RClass* network_module, mrb_state* mrb);

}  // namespace redgem
