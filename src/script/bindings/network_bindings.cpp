// Network natives: hosting and joining a game, sending messages, what the session is like.

#include "script/bindings/network_bindings.hpp"

#include <mruby.h>

#include "core/log.hpp"
#include "net/discovery/lan_discovery.hpp"
#include "net/network_session.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"
#include "script/bindings/message_values.hpp"

namespace redgem {
namespace {

using namespace binding;

constexpr mrb_int kLargestPort = 65535;
constexpr const char* kUnavailable = "Network is only available while a game runs";

bool is_a_port(mrb_int port) { return port >= 0 && port <= kLargestPort; }

// What a native answers when what it was asked to start did not: false, with the reason logged.
mrb_value not_started(const std::string& failure, const std::string& error) {
  log::warning(failure + ": " + error);
  return mrb_false_value();
}

// Lets machines on the same network find the hosted game, under `name` (the game's title when empty).
void announce_hosted_game(mrb_state* mrb, const char* name, mrb_int discovery_port) {
  const std::string shown = *name != '\0' ? std::string(name) : context(mrb).game_title;
  if (!discovery(mrb).announce(shown, network(mrb).port(), port_arg(mrb, discovery_port))) {
    log::warning("the game is hosted, but machines nearby will not find it by themselves: they join by its address");
  }
}

// How many machines from one address may wait to be let in at once, as a script asked.
int waiting_per_address(mrb_state* mrb, mrb_int asked) {
  return static_cast<int>(from_to(mrb, static_cast<mrb_float>(asked), 1.0,
                                  static_cast<mrb_float>(net::kMaxWaitingLinks), "Network.host's waiting_per_address"));
}

// True when the game is being hosted; false (with the reason logged) when the port cannot be used.
// `visible` lets machines on the same network find it.
mrb_value network_host(mrb_state* mrb, mrb_value) {
  mrb_int port = 0, discovery_port = 0, waiting = 0;
  const char* name = nullptr;
  mrb_bool visible = true;
  mrb_get_args(mrb, "izbii", &port, &name, &visible, &discovery_port, &waiting);
  const int per_address = waiting_per_address(mrb, waiting);
  std::string error;
  discovery(mrb).stop_announcing();  // whatever game was hosted before is no longer there to find
  if (!network(mrb).host(port_arg(mrb, port), error, per_address)) {
    return not_started("cannot host a game on port " + std::to_string(port), error);
  }
  if (visible) announce_hosted_game(mrb, name, discovery_port);
  return mrb_true_value();
}

// Opens a room on a relay, or joins one by its code. True when it started (the outcome comes as an event).
mrb_value network_host_room(mrb_state* mrb, mrb_value) {
  const char* address = nullptr;
  mrb_int port = 0;
  mrb_get_args(mrb, "zi", &address, &port);
  std::string error;
  if (network(mrb).host_room(address, port_arg(mrb, port), error)) return mrb_true_value();
  return not_started(std::string("cannot reach the relay at ") + address, error);
}

mrb_value network_join_room(mrb_state* mrb, mrb_value) {
  const char* address = nullptr;
  const char* code = nullptr;
  mrb_int port = 0;
  mrb_value auth;
  mrb_get_args(mrb, "zizo", &address, &port, &code, &auth);
  std::string error;
  if (network(mrb).join_room(address, port_arg(mrb, port), code, error, auth_arg(mrb, auth))) return mrb_true_value();
  return not_started(std::string("cannot join room ") + code + " at " + address, error);
}

// The code of the room this game is in, or nil for a game hosted directly.
mrb_value network_room(mrb_state* mrb, mrb_value) {
  const std::string& room = network(mrb).room();
  return room.empty() ? mrb_nil_value() : text_value(mrb, room);
}

// True when the join started (its outcome comes as an event); false when the address means nothing.
mrb_value network_join(mrb_state* mrb, mrb_value) {
  const char* address = nullptr;
  mrb_int port = 0;
  mrb_value auth;
  mrb_get_args(mrb, "zio", &address, &port, &auth);
  std::string error;
  if (network(mrb).join(address, port_arg(mrb, port), error, auth_arg(mrb, auth))) return mrb_true_value();
  return not_started(std::string("cannot join ") + address, error);
}

mrb_value network_leave(mrb_state* mrb, mrb_value) {
  discovery(mrb).stop_announcing();
  network(mrb).leave();
  return mrb_nil_value();
}

mrb_value network_emit(mrb_state* mrb, mrb_value) {
  const char* name = nullptr;
  mrb_value data;
  mrb_int to = 0;
  mrb_get_args(mrb, "zoi", &name, &data, &to);
  if (std::string(name).size() > net::kMaxNameBytes) mrb_raise(mrb, E_ARGUMENT_ERROR, "a message's name is too long");
  return mrb_bool_value(network(mrb).send(name, json_from(mrb, data), static_cast<int>(to)));
}

void install_connection_bindings(RClass* network_module, mrb_state* mrb) {
  mrb_define_module_function(mrb, network_module, "__host", network_host, MRB_ARGS_REQ(5));
  mrb_define_module_function(mrb, network_module, "__host_room", network_host_room, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, network_module, "__join_room", network_join_room, MRB_ARGS_REQ(4));
  mrb_define_module_function(mrb, network_module, "room", network_room, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, network_module, "__join", network_join, MRB_ARGS_REQ(3));
  mrb_define_module_function(mrb, network_module, "__emit", network_emit, MRB_ARGS_REQ(3));
  mrb_define_module_function(mrb, network_module, "__leave", network_leave, MRB_ARGS_NONE());
}

}  // namespace

net::NetworkSession& network(mrb_state* mrb) { return binding::required(mrb, context(mrb).network, kUnavailable); }
net::LanDiscovery& discovery(mrb_state* mrb) { return binding::required(mrb, context(mrb).discovery, kUnavailable); }

std::uint16_t port_arg(mrb_state* mrb, mrb_int port) {
  if (!is_a_port(port)) mrb_raise(mrb, E_ARGUMENT_ERROR, "a port is a number from 0 to 65535");
  return static_cast<std::uint16_t>(port);
}

void install_network_bindings(mrb_state* mrb) {
  RClass* network_module = mrb_define_module(mrb, "Network");
  install_connection_bindings(network_module, mrb);
  install_network_approval_bindings(network_module, mrb);
  install_network_search_bindings(network_module, mrb);
  install_network_state_bindings(network_module, mrb);
}

}  // namespace redgem
