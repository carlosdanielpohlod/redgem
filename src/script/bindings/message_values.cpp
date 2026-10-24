#include "script/bindings/message_values.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <string>

#include <mruby/array.h>
#include <mruby/hash.h>

#include "net/network_session.hpp"
#include "script/bindings/binding_helpers.hpp"

namespace redgem {
namespace {

using namespace binding;

constexpr const char* kVectorKey = "$v";  // a Vector3 travels as {"$v": [x, y, z]}
constexpr std::size_t kVectorComponents = 3;

nlohmann::json nested_json(mrb_state* mrb, mrb_value value, int depth, const ValueHolder& holder);

// Raises an ArgumentError saying `text`.
[[noreturn]] void refuse(mrb_state* mrb, const std::string& text) { mrb_raise(mrb, E_ARGUMENT_ERROR, text.c_str()); }

// nil, true / false, numbers and names; nullopt for anything else.
std::optional<nlohmann::json> scalar_json(mrb_state* mrb, mrb_value value, const ValueHolder& holder) {
  const bool flag = mrb_true_p(value);
  const bool is_flag = flag || mrb_false_p(value);
  if (mrb_nil_p(value)) return nlohmann::json(nullptr);
  if (is_flag) return nlohmann::json(flag);
  if (mrb_integer_p(value)) return nlohmann::json(static_cast<std::int64_t>(mrb_integer(value)));
  if (mrb_float_p(value)) {
    const std::string refusal = std::string(holder.name) + " cannot " + holder.verb + " infinity or NaN";
    return nlohmann::json(finite_number(mrb, value, refusal.c_str()));
  }
  if (is_name(value)) return nlohmann::json(to_name(mrb, value));
  return std::nullopt;
}

// An array or a hash of a message holds only so much: the other machines would refuse the message.
void check_members(mrb_state* mrb, mrb_int count, const ValueHolder& holder) {
  if (static_cast<std::size_t>(count) > holder.most_members) {
    refuse(mrb, std::string("an array or a hash of ") + holder.name + " holds too many things");
  }
}

nlohmann::json array_json(mrb_state* mrb, mrb_value value, int depth, const ValueHolder& holder) {
  nlohmann::json list = nlohmann::json::array();
  check_members(mrb, RARRAY_LEN(value), holder);
  for (mrb_int i = 0; i < RARRAY_LEN(value); ++i) {
    list.push_back(nested_json(mrb, mrb_ary_entry(value, i), depth + 1, holder));
  }
  return list;
}

// A hash's key as a message carries it, which must be a name of a sensible length.
std::string key_name(mrb_state* mrb, mrb_value key, const ValueHolder& holder) {
  if (!is_name(key)) refuse(mrb, std::string(holder.owner) + " hash keys must be names");
  const std::string name = to_name(mrb, key);
  if (name.size() > net::kMaxNameBytes) refuse(mrb, std::string(holder.owner) + " hash key is too long");
  return name;
}

nlohmann::json hash_json(mrb_state* mrb, mrb_value value, int depth, const ValueHolder& holder) {
  nlohmann::json object = nlohmann::json::object();
  const mrb_value keys = mrb_hash_keys(mrb, value);
  check_members(mrb, RARRAY_LEN(keys), holder);
  for (mrb_int i = 0; i < RARRAY_LEN(keys); ++i) {
    const mrb_value key = mrb_ary_entry(keys, i);
    const std::string name = key_name(mrb, key, holder);
    object[name] = nested_json(mrb, mrb_hash_get(mrb, value, key), depth + 1, holder);
  }
  return object;
}

nlohmann::json vector_json(mrb_state* mrb, mrb_value value) {
  const Vec3 vector = to_vec3(mrb, value);
  return {{kVectorKey, {vector.x, vector.y, vector.z}}};
}

nlohmann::json nested_json(mrb_state* mrb, mrb_value value, int depth, const ValueHolder& holder) {
  if (depth > net::kMaxDataDepth) refuse(mrb, std::string(holder.owner) + " data is nested too deep");
  if (const auto scalar = scalar_json(mrb, value, holder)) return *scalar;
  if (mrb_array_p(value)) return array_json(mrb, value, depth, holder);
  if (mrb_hash_p(value)) return hash_json(mrb, value, depth, holder);
  if (is_vector3(mrb, value)) return vector_json(mrb, value);
  mrb_raisef(mrb, E_ARGUMENT_ERROR, "%s cannot %s %v: use numbers, texts, true/false, Vector3, arrays and hashes",
             holder.name, holder.verb, mrb_inspect(mrb, value));
  return nullptr;
}

// Three numbers under the vector's key, and nothing else.
bool holds_only_a_vector(const nlohmann::json& json) {
  return json.is_object() && json.size() == 1 && json.contains(kVectorKey);
}

// A number a Vector3 can hold: converting one past a float's range to a float is undefined.
bool fits_a_float(const nlohmann::json& part) {
  if (!part.is_number()) return false;
  const double number = part.get<double>();
  return std::isfinite(number) && std::fabs(number) <= std::numeric_limits<float>::max();
}

bool is_vector(const nlohmann::json& json) {
  if (!holds_only_a_vector(json)) return false;
  const nlohmann::json& parts = json[kVectorKey];
  return parts.is_array() && parts.size() == kVectorComponents && std::all_of(parts.begin(), parts.end(), fits_a_float);
}

mrb_value scalar_value(mrb_state* mrb, const nlohmann::json& json) {
  if (json.is_boolean()) return mrb_bool_value(json.get<bool>());
  if (json.is_number_integer()) return mrb_int_value(mrb, static_cast<mrb_int>(json.get<std::int64_t>()));
  if (json.is_number()) return mrb_float_value(mrb, json.get<double>());
  if (json.is_string()) return text_value(mrb, json.get_ref<const std::string&>());
  return mrb_nil_value();
}

mrb_value array_value(mrb_state* mrb, const nlohmann::json& json, HashKeys keys) {
  const mrb_value list = mrb_ary_new_capa(mrb, static_cast<mrb_int>(json.size()));
  for (const nlohmann::json& item : json) mrb_ary_push(mrb, list, value_from(mrb, item, keys));
  return list;
}

mrb_value vector_value(mrb_state* mrb, const nlohmann::json& json) {
  const auto parts = json[kVectorKey].get<std::array<float, kVectorComponents>>();
  return vec3_value(mrb, vec3_from(parts.data()));
}

// A key of a hash that arrived: the Symbol the game's scripts know it by, or a String for a name
// no script here ever used. Symbols are never forgotten, so making one for every key another
// machine comes up with would let it fill this one's memory.
mrb_value key_value(mrb_state* mrb, const std::string& key, HashKeys keys) {
  const mrb_sym known = keys == HashKeys::KnownSymbols ? mrb_intern_check(mrb, key.data(), key.size()) : 0;
  return known != 0 ? mrb_symbol_value(known) : text_value(mrb, key);
}

mrb_value hash_value(mrb_state* mrb, const nlohmann::json& json, HashKeys keys) {
  const mrb_value hash = mrb_hash_new(mrb);
  for (const auto& [key, item] : json.items()) {
    mrb_hash_set(mrb, hash, key_value(mrb, key, keys), value_from(mrb, item, keys));
  }
  return hash;
}

}  // namespace

nlohmann::json json_from(mrb_state* mrb, mrb_value value, const ValueHolder& holder) {
  return nested_json(mrb, value, 0, holder);
}

mrb_value value_from(mrb_state* mrb, const nlohmann::json& json, HashKeys keys) {
  if (json.is_array()) return array_value(mrb, json, keys);
  if (is_vector(json)) return vector_value(mrb, json);
  if (json.is_object()) return hash_value(mrb, json, keys);
  return scalar_value(mrb, json);
}

}  // namespace redgem
