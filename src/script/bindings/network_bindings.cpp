// Network natives, and the values messages carry.

#include "script/bindings/network_bindings.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

#include <mruby/array.h>
#include <mruby/hash.h>
#include <mruby/string.h>

#include "core/log.hpp"
#include "net/lan_discovery.hpp"
#include "net/network_session.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using namespace binding;

constexpr const char* kVectorKey = "$v";  // a Vector3 travels as {"$v": [x, y, z]}
constexpr std::size_t kVectorComponents = 3;
constexpr mrb_int kLargestPort = 65535;

nlohmann::json nested_json(mrb_state* mrb, mrb_value value, int depth);

double finite_number(mrb_state* mrb, mrb_value value) {
  if (!std::isfinite(mrb_float(value))) mrb_raise(mrb, E_ARGUMENT_ERROR, "a message cannot carry infinity or NaN");
  return static_cast<double>(mrb_float(value));
}

// nil, true / false, numbers and names; nullopt for anything else.
std::optional<nlohmann::json> scalar_json(mrb_state* mrb, mrb_value value) {
  const bool flag = mrb_true_p(value);
  if (mrb_nil_p(value)) return nlohmann::json(nullptr);
  if (flag || mrb_false_p(value)) return nlohmann::json(flag);
  if (mrb_integer_p(value)) return nlohmann::json(static_cast<std::int64_t>(mrb_integer(value)));
  if (mrb_float_p(value)) return nlohmann::json(finite_number(mrb, value));
  if (is_name(value)) return nlohmann::json(to_name(mrb, value));
  return std::nullopt;
}

// An array or a hash holds only so much: the other machines would refuse the message.
void check_members(mrb_state* mrb, mrb_int count) {
  if (count > static_cast<mrb_int>(net::kMaxDataMembers)) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "an array or a hash of a message holds too many things");
  }
}

nlohmann::json array_json(mrb_state* mrb, mrb_value value, int depth) {
  nlohmann::json list = nlohmann::json::array();
  check_members(mrb, RARRAY_LEN(value));
  for (mrb_int i = 0; i < RARRAY_LEN(value); ++i) list.push_back(nested_json(mrb, mrb_ary_entry(value, i), depth + 1));
  return list;
}

// A hash's key as a message carries it, which must be a name of a sensible length.
std::string key_name(mrb_state* mrb, mrb_value key) {
  if (!is_name(key)) mrb_raise(mrb, E_ARGUMENT_ERROR, "a message's hash keys must be names");
  const std::string name = to_name(mrb, key);
  if (name.size() > net::kMaxNameBytes) mrb_raise(mrb, E_ARGUMENT_ERROR, "a message's hash key is too long");
  return name;
}

nlohmann::json hash_json(mrb_state* mrb, mrb_value value, int depth) {
  nlohmann::json object = nlohmann::json::object();
  const mrb_value keys = mrb_hash_keys(mrb, value);
  check_members(mrb, RARRAY_LEN(keys));
  for (mrb_int i = 0; i < RARRAY_LEN(keys); ++i) {
    const mrb_value key = mrb_ary_entry(keys, i);
    const std::string name = key_name(mrb, key);
    object[name] = nested_json(mrb, mrb_hash_get(mrb, value, key), depth + 1);
  }
  return object;
}

nlohmann::json vector_json(mrb_state* mrb, mrb_value value) {
  const Vec3 vector = to_vec3(mrb, value);
  return {{kVectorKey, {vector.x, vector.y, vector.z}}};
}

nlohmann::json nested_json(mrb_state* mrb, mrb_value value, int depth) {
  if (depth > net::kMaxDataDepth) mrb_raise(mrb, E_ARGUMENT_ERROR, "a message's data is nested too deep");
  if (const auto scalar = scalar_json(mrb, value)) return *scalar;
  if (mrb_array_p(value)) return array_json(mrb, value, depth);
  if (mrb_hash_p(value)) return hash_json(mrb, value, depth);
  if (is_vector3(mrb, value)) return vector_json(mrb, value);
  mrb_raisef(mrb, E_ARGUMENT_ERROR,
             "a message cannot carry %v: use numbers, texts, true/false, Vector3, arrays and hashes",
             mrb_inspect(mrb, value));
  return nullptr;
}

bool is_vector(const nlohmann::json& json) {
  if (!json.is_object() || json.size() != 1 || !json.contains(kVectorKey)) return false;
  const nlohmann::json& parts = json[kVectorKey];
  return parts.is_array() && parts.size() == kVectorComponents &&
         std::all_of(parts.begin(), parts.end(), [](const nlohmann::json& part) { return part.is_number(); });
}

mrb_value scalar_value(mrb_state* mrb, const nlohmann::json& json) {
  if (json.is_boolean()) return mrb_bool_value(json.get<bool>());
  if (json.is_number_integer()) return mrb_int_value(mrb, static_cast<mrb_int>(json.get<std::int64_t>()));
  if (json.is_number()) return mrb_float_value(mrb, json.get<double>());
  if (json.is_string()) return text_value(mrb, json.get_ref<const std::string&>());
  return mrb_nil_value();
}

mrb_value array_value(mrb_state* mrb, const nlohmann::json& json) {
  const mrb_value list = mrb_ary_new_capa(mrb, static_cast<mrb_int>(json.size()));
  for (const nlohmann::json& item : json) mrb_ary_push(mrb, list, value_from(mrb, item));
  return list;
}

mrb_value vector_value(mrb_state* mrb, const nlohmann::json& json) {
  const auto parts = json[kVectorKey].get<std::array<float, kVectorComponents>>();
  return vec3_value(mrb, vec3_from(parts.data()));
}

// A key of a hash that arrived: the Symbol the game's scripts know it by, or a String for a name
// no script here ever used. Symbols are never forgotten, so making one for every key another
// machine comes up with would let it fill this one's memory.
mrb_value key_value(mrb_state* mrb, const std::string& key) {
  const mrb_sym known = mrb_intern_check(mrb, key.data(), key.size());
  return known != 0 ? mrb_symbol_value(known) : text_value(mrb, key);
}

mrb_value hash_value(mrb_state* mrb, const nlohmann::json& json) {
  const mrb_value hash = mrb_hash_new(mrb);
  for (const auto& [key, item] : json.items()) mrb_hash_set(mrb, hash, key_value(mrb, key), value_from(mrb, item));
  return hash;
}

net::NetworkSession& network(mrb_state* mrb) {
  net::NetworkSession* session = context(mrb).network;
  if (session == nullptr) mrb_raise(mrb, E_RUNTIME_ERROR, "Network is only available while a game runs");
  return *session;
}

std::uint16_t port_arg(mrb_state* mrb, mrb_int port) {
  if (port < 0 || port > kLargestPort) mrb_raise(mrb, E_ARGUMENT_ERROR, "a port is a number from 0 to 65535");
  return static_cast<std::uint16_t>(port);
}

net::LanDiscovery& discovery(mrb_state* mrb) {
  net::LanDiscovery* found = context(mrb).discovery;
  if (found == nullptr) mrb_raise(mrb, E_RUNTIME_ERROR, "Network is only available while a game runs");
  return *found;
}

// Lets machines on the same network find the hosted game, under `name` (the game's title when empty).
void announce_hosted_game(mrb_state* mrb, const char* name, mrb_int discovery_port) {
  const std::string shown = *name != '\0' ? std::string(name) : context(mrb).game_title;
  if (!discovery(mrb).announce(shown, network(mrb).port(), port_arg(mrb, discovery_port))) {
    log::warning("the game is hosted, but machines nearby will not find it by themselves: they join by its address");
  }
}

// True when the game is being hosted; false (with the reason logged) when the port cannot be used.
// `visible` lets machines on the same network find it.
mrb_value network_host(mrb_state* mrb, mrb_value) {
  mrb_int port = 0, discovery_port = 0;
  const char* name = nullptr;
  mrb_bool visible = true;
  mrb_get_args(mrb, "izbi", &port, &name, &visible, &discovery_port);
  std::string error;
  discovery(mrb).stop_announcing();  // whatever game was hosted before is no longer there to find
  if (!network(mrb).host(port_arg(mrb, port), error)) {
    log::warning("cannot host a game on port " + std::to_string(port) + ": " + error);
    return mrb_false_value();
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
  log::warning(std::string("cannot reach the relay at ") + address + ": " + error);
  return mrb_false_value();
}

mrb_value network_join_room(mrb_state* mrb, mrb_value) {
  const char* address = nullptr;
  const char* code = nullptr;
  mrb_int port = 0;
  mrb_get_args(mrb, "ziz", &address, &port, &code);
  std::string error;
  if (network(mrb).join_room(address, port_arg(mrb, port), code, error)) return mrb_true_value();
  log::warning(std::string("cannot join room ") + code + " at " + address + ": " + error);
  return mrb_false_value();
}

// The code of the room this game is in, or nil for a game hosted directly.
mrb_value network_room(mrb_state* mrb, mrb_value) {
  const std::string& room = network(mrb).room();
  return room.empty() ? mrb_nil_value() : text_value(mrb, room);
}

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

// True when the join started (its outcome comes as an event); false when the address means nothing.
mrb_value network_join(mrb_state* mrb, mrb_value) {
  const char* address = nullptr;
  mrb_int port = 0;
  mrb_get_args(mrb, "zi", &address, &port);
  std::string error;
  if (network(mrb).join(address, port_arg(mrb, port), error)) return mrb_true_value();
  log::warning(std::string("cannot join ") + address + ": " + error);
  return mrb_false_value();
}

mrb_value network_leave(mrb_state* mrb, mrb_value) {
  discovery(mrb).stop_announcing();
  network(mrb).leave();
  return mrb_nil_value();
}

mrb_value network_online(mrb_state* mrb, mrb_value) {
  return mrb_bool_value(network(mrb).state() == net::NetworkSession::State::Online);
}
mrb_value network_connecting(mrb_state* mrb, mrb_value) {
  return mrb_bool_value(network(mrb).state() == net::NetworkSession::State::Connecting);
}
mrb_value network_hosting(mrb_state* mrb, mrb_value) { return mrb_bool_value(network(mrb).hosting()); }
mrb_value network_id(mrb_state* mrb, mrb_value) { return mrb_int_value(mrb, network(mrb).id()); }
mrb_value network_port(mrb_state* mrb, mrb_value) { return mrb_int_value(mrb, network(mrb).port()); }

mrb_value network_peers(mrb_state* mrb, mrb_value) {
  const mrb_value peers = mrb_ary_new(mrb);
  for (const int peer : network(mrb).peers()) mrb_ary_push(mrb, peers, mrb_int_value(mrb, peer));
  return peers;
}

mrb_value network_emit(mrb_state* mrb, mrb_value) {
  const char* name = nullptr;
  mrb_value data;
  mrb_int to = 0;
  mrb_get_args(mrb, "zoi", &name, &data, &to);
  if (std::string(name).size() > net::kMaxNameBytes) mrb_raise(mrb, E_ARGUMENT_ERROR, "a message's name is too long");
  return mrb_bool_value(network(mrb).send(name, json_from(mrb, data), static_cast<int>(to)));
}

}  // namespace

nlohmann::json json_from(mrb_state* mrb, mrb_value value) { return nested_json(mrb, value, 0); }

mrb_value value_from(mrb_state* mrb, const nlohmann::json& json) {
  if (json.is_array()) return array_value(mrb, json);
  if (is_vector(json)) return vector_value(mrb, json);
  if (json.is_object()) return hash_value(mrb, json);
  return scalar_value(mrb, json);
}

namespace {

void install_connection_bindings(RClass* network_module, mrb_state* mrb) {
  mrb_define_module_function(mrb, network_module, "__host", network_host, MRB_ARGS_REQ(4));
  mrb_define_module_function(mrb, network_module, "__host_room", network_host_room, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, network_module, "__join_room", network_join_room, MRB_ARGS_REQ(3));
  mrb_define_module_function(mrb, network_module, "room", network_room, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, network_module, "__join", network_join, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, network_module, "__emit", network_emit, MRB_ARGS_REQ(3));
  mrb_define_module_function(mrb, network_module, "__leave", network_leave, MRB_ARGS_NONE());
}

// Finding games on the local network.
void install_search_bindings(RClass* network_module, mrb_state* mrb) {
  mrb_define_module_function(mrb, network_module, "__search", network_search, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, network_module, "searching?", network_searching, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, network_module, "stop_search", network_stop_search, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, network_module, "games", network_games, MRB_ARGS_NONE());
}

// What the session is like right now.
void install_state_bindings(RClass* network_module, mrb_state* mrb) {
  mrb_define_module_function(mrb, network_module, "online?", network_online, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, network_module, "connecting?", network_connecting, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, network_module, "host?", network_hosting, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, network_module, "id", network_id, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, network_module, "port", network_port, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, network_module, "peers", network_peers, MRB_ARGS_NONE());
}

}  // namespace

void install_network_bindings(mrb_state* mrb) {
  RClass* network_module = mrb_define_module(mrb, "Network");
  install_connection_bindings(network_module, mrb);
  install_search_bindings(network_module, mrb);
  install_state_bindings(network_module, mrb);
}

}  // namespace redgem
