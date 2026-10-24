#include "input/input_map.hpp"

#include <algorithm>

namespace redgem {
namespace {

// What starts the text of each kind of binding: "key:space", "pad:a".
constexpr const char* kKeyPrefix = "key:";
constexpr const char* kPadPrefix = "pad:";

bool starts_with(const std::string& text, const std::string& prefix) { return text.rfind(prefix, 0) == 0; }

template <typename Value>
void add_once(std::vector<Value>& values, Value value) {
  if (std::find(values.begin(), values.end(), value) == values.end()) values.push_back(value);
}

template <typename Value>
void remove_all(std::vector<Value>& values, Value value) {
  values.erase(std::remove(values.begin(), values.end(), value), values.end());
}

ActionBinding buttons_only(const ActionBinding& binding) { return {{}, binding.buttons}; }

}  // namespace

std::optional<InputBinding> binding_from_text(const std::string& text) {
  const std::string key_prefix = kKeyPrefix;
  const std::string pad_prefix = kPadPrefix;
  if (starts_with(text, key_prefix)) {
    if (const auto key = key_from_name(text.substr(key_prefix.size()))) return InputBinding(*key);
  }
  if (starts_with(text, pad_prefix)) {
    if (const auto button = gamepad_button_from_name(text.substr(pad_prefix.size()))) return InputBinding(*button);
  }
  return std::nullopt;
}

std::string binding_text(const InputBinding& binding) {
  if (const Key* key = std::get_if<Key>(&binding)) return kKeyPrefix + key_name(*key);
  return kPadPrefix + gamepad_button_name(std::get<GamepadButton>(binding));
}

void ActionBinding::add(const InputBinding& binding) {
  if (const Key* key = std::get_if<Key>(&binding)) return add_once(keys, *key);
  add_once(buttons, std::get<GamepadButton>(binding));
}

void ActionBinding::remove(const InputBinding& binding) {
  if (const Key* key = std::get_if<Key>(&binding)) return remove_all(keys, *key);
  remove_all(buttons, std::get<GamepadButton>(binding));
}

std::vector<InputBinding> ActionBinding::all() const {
  std::vector<InputBinding> out(keys.begin(), keys.end());
  out.insert(out.end(), buttons.begin(), buttons.end());
  return out;
}

InputMap InputMap::defaults() {
  InputMap map;
  map.bind_axis("horizontal", {{{Key::A, Key::Left}, {}}, {{Key::D, Key::Right}, {}}, GamepadAxis::LeftX, false});
  map.bind_axis("vertical", {{{Key::S, Key::Down}, {}}, {{Key::W, Key::Up}, {}}, GamepadAxis::LeftY, true});
  map.bind_action("jump", {{Key::Space}, {GamepadButton::A}});
  map.bind_action("fire", {{Key::LeftControl, Key::Enter}, {GamepadButton::X}});
  map.bind_action("run", {{Key::LeftShift}, {GamepadButton::B}});
  return map;
}

const AxisBinding* InputMap::axis(const std::string& name) const {
  const auto it = axes_.find(name);
  return it == axes_.end() ? nullptr : &it->second;
}

const ActionBinding* InputMap::action(const std::string& name) const {
  const auto it = actions_.find(name);
  return it == actions_.end() ? nullptr : &it->second;
}

void InputMap::forget(const std::string& name) {
  axes_.erase(name);
  actions_.erase(name);
}

InputMap InputMap::without_keys() const {
  InputMap map;
  for (const auto& [name, binding] : actions_) map.bind_action(name, buttons_only(binding));
  for (const auto& [name, binding] : axes_) {
    map.bind_axis(
        name, {buttons_only(binding.negative), buttons_only(binding.positive), binding.stick, binding.stick_inverted});
  }
  return map;
}

}  // namespace redgem
