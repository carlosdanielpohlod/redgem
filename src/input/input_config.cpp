#include "input/input_config.hpp"

namespace redgem {
namespace {

constexpr const char* kActionsKey = "actions";
constexpr const char* kAxesKey = "axes";
constexpr const char* kNegativeKey = "negative";
constexpr const char* kPositiveKey = "positive";
constexpr const char* kStickKey = "pad_axis";
constexpr const char* kInvertKey = "invert";

using Json = nlohmann::json;
using Problems = std::vector<std::string>;

bool holds_object(const Json& json, const char* key) {
  return json.is_object() && json.contains(key) && json[key].is_object();
}

// Whether an axis says its stick runs the other way (anything but true says it does not).
bool says_invert(const Json& axis) { return axis.contains(kInvertKey) && axis[kInvertKey] == true; }

// A list of bindings, as text, under `what` (an action's name, or an axis' and its side).
ActionBinding bindings_from(const Json& list, const std::string& what, Problems& problems) {
  ActionBinding binding;
  if (!list.is_array()) {
    problems.push_back("input: " + what + " is not a list of bindings");
    return binding;
  }
  for (const Json& entry : list) {
    const auto one = entry.is_string() ? binding_from_text(entry.get<std::string>()) : std::nullopt;
    if (one) binding.add(*one);
    if (!one) problems.push_back("input: " + what + " has an unknown binding " + entry.dump());
  }
  return binding;
}

// A side of an axis, which it may leave out (a stick alone moves it).
ActionBinding side_from(const Json& axis, const char* side, const std::string& name, Problems& problems) {
  if (!axis.contains(side)) return {};
  return bindings_from(axis[side], name + " " + side, problems);
}

void read_stick(const Json& axis, const std::string& name, AxisBinding& binding, Problems& problems) {
  if (axis.contains(kStickKey)) {
    const Json& stick = axis[kStickKey];
    binding.stick = stick.is_string() ? gamepad_axis_from_name(stick.get<std::string>()) : std::nullopt;
    if (!binding.stick) problems.push_back("input: " + name + " has an unknown pad_axis " + stick.dump());
  }
  binding.stick_inverted = says_invert(axis);
}

void read_axis(const std::string& name, const Json& axis, InputMap& map, Problems& problems) {
  if (!axis.is_object()) {
    problems.push_back("input: axis " + name + " is not an object");
    return;
  }
  AxisBinding binding;
  binding.negative = side_from(axis, kNegativeKey, name, problems);
  binding.positive = side_from(axis, kPositiveKey, name, problems);
  read_stick(axis, name, binding, problems);
  map.bind_axis(name, std::move(binding));
}

Json bindings_json(const ActionBinding& binding) {
  Json list = Json::array();
  for (const InputBinding& one : binding.all()) list.push_back(binding_text(one));
  return list;
}

Json axis_json(const AxisBinding& axis) {
  Json json = {{kNegativeKey, bindings_json(axis.negative)}, {kPositiveKey, bindings_json(axis.positive)}};
  if (axis.stick) json[kStickKey] = gamepad_axis_name(*axis.stick);
  if (axis.stick_inverted) json[kInvertKey] = true;
  return json;
}

}  // namespace

InputMap input_map_from_json(const Json& section, Problems& problems) {
  InputMap map;
  if (holds_object(section, kActionsKey)) {
    for (const auto& [name, list] : section[kActionsKey].items()) {
      map.bind_action(name, bindings_from(list, name, problems));
    }
  }
  if (holds_object(section, kAxesKey)) {
    for (const auto& [name, axis] : section[kAxesKey].items()) read_axis(name, axis, map, problems);
  }
  return map;
}

Json input_map_json(const InputMap& map) {
  Json actions = Json::object();
  for (const auto& [name, binding] : map.actions()) actions[name] = bindings_json(binding);
  Json axes = Json::object();
  for (const auto& [name, axis] : map.axes()) axes[name] = axis_json(axis);
  return {{kActionsKey, actions}, {kAxesKey, axes}};
}

}  // namespace redgem
