// Network natives: finding games on the local network.

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/hash.h>
#include <mruby/string.h>

#include "net/discovery/lan_discovery.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/network_bindings.hpp"

namespace redgem {
namespace {

using namespace binding;

mrb_value network_search(mrb_state* mrb, mrb_value) {
  mrb_int discovery_port = 0;
  mrb_get_args(mrb, "i", &discovery_port);
  return mrb_bool_value(discovery(mrb).search(port_arg(mrb, discovery_port)));
}

mrb_value network_searching(mrb_state* mrb, mrb_value) { return mrb_bool_value(discovery(mrb).searching()); }

mrb_value network_stop_search(mrb_state* mrb, mrb_value) {
  discovery(mrb).stop_searching();
  return mrb_nil_value();
}

// The games found on the local network: [{ name:, address:, port:, players: }, ...].
mrb_value network_games(mrb_state* mrb, mrb_value) {
  const mrb_value games = mrb_ary_new(mrb);
  for (const net::NearbyGame& game : discovery(mrb).games()) {
    const mrb_value entry = mrb_hash_new(mrb);
    set_entry(mrb, entry, "name", text_value(mrb, game.name));
    set_entry(mrb, entry, "address", mrb_str_new_cstr(mrb, game.address.c_str()));
    set_entry(mrb, entry, "port", mrb_int_value(mrb, game.port));
    set_entry(mrb, entry, "players", mrb_int_value(mrb, game.players));
    mrb_ary_push(mrb, games, entry);
  }
  return games;
}

}  // namespace

void install_network_search_bindings(RClass* network_module, mrb_state* mrb) {
  mrb_define_module_function(mrb, network_module, "__search", network_search, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, network_module, "searching?", network_searching, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, network_module, "stop_search", network_stop_search, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, network_module, "games", network_games, MRB_ARGS_NONE());
}

}  // namespace redgem
