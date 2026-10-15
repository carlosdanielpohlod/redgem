// Network natives: what the session is like right now.

#include <mruby.h>
#include <mruby/array.h>

#include <vector>

#include "net/network_session.hpp"
#include "script/bindings/network_bindings.hpp"

namespace redgem {
namespace {

mrb_value ids_value(mrb_state* mrb, const std::vector<int>& ids) {
  const mrb_value list = mrb_ary_new(mrb);
  for (const int id : ids) mrb_ary_push(mrb, list, mrb_int_value(mrb, id));
  return list;
}

mrb_value network_online(mrb_state* mrb, mrb_value) {
  return mrb_bool_value(network(mrb).state() == net::NetworkSession::State::Online);
}
mrb_value network_connecting(mrb_state* mrb, mrb_value) {
  return mrb_bool_value(network(mrb).state() == net::NetworkSession::State::Connecting);
}
mrb_value network_hosting(mrb_state* mrb, mrb_value) { return mrb_bool_value(network(mrb).hosting()); }
mrb_value network_serving(mrb_state* mrb, mrb_value) { return mrb_bool_value(network(mrb).serving()); }
mrb_value network_id(mrb_state* mrb, mrb_value) { return mrb_int_value(mrb, network(mrb).id()); }
mrb_value network_port(mrb_state* mrb, mrb_value) { return mrb_int_value(mrb, network(mrb).port()); }
mrb_value network_peers(mrb_state* mrb, mrb_value) { return ids_value(mrb, network(mrb).peers()); }
mrb_value network_players(mrb_state* mrb, mrb_value) { return ids_value(mrb, network(mrb).players()); }

}  // namespace

void install_network_state_bindings(RClass* network_module, mrb_state* mrb) {
  mrb_define_module_function(mrb, network_module, "online?", network_online, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, network_module, "connecting?", network_connecting, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, network_module, "host?", network_hosting, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, network_module, "server?", network_serving, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, network_module, "id", network_id, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, network_module, "port", network_port, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, network_module, "peers", network_peers, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, network_module, "players", network_players, MRB_ARGS_NONE());
}

}  // namespace redgem
