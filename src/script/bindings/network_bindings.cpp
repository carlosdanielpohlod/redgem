// Network natives, and the values messages carry.

#include "script/bindings/network_bindings.hpp"

#include <cmath>

#include <mruby/array.h>
#include <mruby/hash.h>
#include <mruby/string.h>

#include "core/log.hpp"
#include "net/network_session.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using namespace binding;

constexpr const char* kVectorKey = "$v";   // a Vector3 travels as {"$v": [x, y, z]}
constexpr std::size_t kVectorComponents = 3;
constexpr mrb_int kLargestPort = 65535;

nlohmann::json nested_json(mrb_state* mrb, mrb_value value, int depth) {
  if (depth > net::kMaxDataDepth) mrb_raise(mrb, E_ARGUMENT_ERROR, "a message's data is nested too deep");
  if (mrb_nil_p(value)) return nullptr;
  if (mrb_true_p(value)) return true;
  if (mrb_false_p(value)) return false;
  if (mrb_integer_p(value)) return static_cast<std::int64_t>(mrb_integer(value));
  if (mrb_float_p(value)) {
    if (!std::isfinite(mrb_float(value))) mrb_raise(mrb, E_ARGUMENT_ERROR, "a message cannot carry infinity or NaN");
    return static_cast<double>(mrb_float(value));
  }
  if (mrb_string_p(value) || mrb_symbol_p(value)) return to_name(mrb, value);
  if (mrb_array_p(value)) {
    nlohmann::json list = nlohmann::json::array();
    for (mrb_int i = 0; i < RARRAY_LEN(value); ++i) list.push_back(nested_json(mrb, mrb_ary_entry(value, i), depth + 1));
    return list;
  }
  if (mrb_hash_p(value)) {
    nlohmann::json object = nlohmann::json::object();
    const mrb_value keys = mrb_hash_keys(mrb, value);
    for (mrb_int i = 0; i < RARRAY_LEN(keys); ++i) {
      const mrb_value key = mrb_ary_entry(keys, i);
      if (!mrb_string_p(key) && !mrb_symbol_p(key)) mrb_raise(mrb, E_ARGUMENT_ERROR, "a message's hash keys must be names");
      const std::string name = to_name(mrb, key);
      if (name.size() > net::kMaxNameBytes) mrb_raise(mrb, E_ARGUMENT_ERROR, "a message's hash key is too long");
      object[name] = nested_json(mrb, mrb_hash_get(mrb, value, key), depth + 1);
    }
    return object;
  }
  if (mrb_obj_is_kind_of(mrb, value, mrb_class_get(mrb, "Vector3"))) {
    const Vec3 vector = to_vec3(mrb, value);
    return {{kVectorKey, {vector.x, vector.y, vector.z}}};
  }
  mrb_raisef(mrb, E_ARGUMENT_ERROR, "a message cannot carry %v: use numbers, texts, true/false, Vector3, arrays and hashes",
             mrb_inspect(mrb, value));
  return nullptr;
}

bool is_vector(const nlohmann::json& json) {
  if (!json.is_object() || json.size() != 1 || !json.contains(kVectorKey)) return false;
  const nlohmann::json& parts = json[kVectorKey];
  return parts.is_array() && parts.size() == kVectorComponents && parts[0].is_number() && parts[1].is_number() &&
         parts[2].is_number();
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

// True when the game is being hosted; false (with the reason logged) when the port cannot be used.
mrb_value network_host(mrb_state* mrb, mrb_value) {
  mrb_int port = 0;
  mrb_get_args(mrb, "i", &port);
  std::string error;
  if (network(mrb).host(port_arg(mrb, port), error)) return mrb_true_value();
  log::warning("cannot host a game on port " + std::to_string(port) + ": " + error);
  return mrb_false_value();
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
  if (json.is_boolean()) return mrb_bool_value(json.get<bool>());
  if (json.is_number_integer()) return mrb_int_value(mrb, static_cast<mrb_int>(json.get<std::int64_t>()));
  if (json.is_number()) return mrb_float_value(mrb, json.get<double>());
  if (json.is_string()) {
    const std::string& text = json.get_ref<const std::string&>();
    return mrb_str_new(mrb, text.data(), static_cast<mrb_int>(text.size()));
  }
  if (json.is_array()) {
    const mrb_value list = mrb_ary_new_capa(mrb, static_cast<mrb_int>(json.size()));
    for (const nlohmann::json& item : json) mrb_ary_push(mrb, list, value_from(mrb, item));
    return list;
  }
  if (is_vector(json)) {
    const nlohmann::json& parts = json[kVectorKey];
    return vec3_value(mrb, {parts[0].get<float>(), parts[1].get<float>(), parts[2].get<float>()});
  }
  if (json.is_object()) {
    const mrb_value hash = mrb_hash_new(mrb);
    for (const auto& [key, item] : json.items()) {
      mrb_hash_set(mrb, hash, mrb_symbol_value(mrb_intern(mrb, key.data(), key.size())), value_from(mrb, item));
    }
    return hash;
  }
  return mrb_nil_value();
}

void install_network_bindings(mrb_state* mrb) {
  RClass* network_module = mrb_define_module(mrb, "Network");
  mrb_define_module_function(mrb, network_module, "__host", network_host, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, network_module, "__join", network_join, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, network_module, "__emit", network_emit, MRB_ARGS_REQ(3));
  mrb_define_module_function(mrb, network_module, "__leave", network_leave, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, network_module, "online?", network_online, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, network_module, "connecting?", network_connecting, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, network_module, "host?", network_hosting, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, network_module, "id", network_id, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, network_module, "port", network_port, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, network_module, "peers", network_peers, MRB_ARGS_NONE());
}

}  // namespace redgem
