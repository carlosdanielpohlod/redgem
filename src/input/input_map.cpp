#include "input/input_map.hpp"

#include <algorithm>

namespace redgem {
namespace {

// What starts the text of each kind of binding: "key:space", "mouse:left", "pad:a".
constexpr const char* kKeyPrefix = "key:";
constexpr const char* kMousePrefix = "mouse:";
constexpr const char* kPadPrefix = "pad:";

bool starts_with(const std::string& text, const std::string& prefix) { return text.rfind(prefix, 0) == 0; }

// The binding a text names, when it starts with `prefix` and the rest is a name `from_name` knows.
template <typename FromName>
std::optional<InputBinding> named_after(const std::string& text, const std::string& prefix, FromName from_name) {
  if (!starts_with(text, prefix)) return std::nullopt;
  const auto value = from_name(text.substr(prefix.size()));
  if (!value) return std::nullopt;
  return InputBinding(*value);
}

std::string text_of(Key key) { return kKeyPrefix + key_name(key); }
std::string text_of(MouseInput input) { return kMousePrefix + mouse_input_name(input); }
std::string text_of(GamepadButton button) { return kPadPrefix + gamepad_button_name(button); }

// The list of an action's bindings that a binding of each kind goes in.
std::vector<Key>& list_for(ActionBinding& binding, Key) { return binding.keys; }
std::vector<MouseInput>& list_for(ActionBinding& binding, MouseInput) { return binding.mouse; }
std::vector<GamepadButton>& list_for(ActionBinding& binding, GamepadButton) { return binding.buttons; }

template <typename Value>
void add_once(std::vector<Value>& values, Value value) {
  if (std::find(values.begin(), values.end(), value) == values.end()) values.push_back(value);
}

template <typename Value>
void remove_all(std::vector<Value>& values, Value value) {
  values.erase(std::remove(values.begin(), values.end(), value), values.end());
}

ActionBinding buttons_only(const ActionBinding& binding) { return {{}, binding.buttons, {}}; }

}  // namespace

std::optional<InputBinding> binding_from_text(const std::string& text) {
  if (auto key = named_after(text, kKeyPrefix, key_from_name)) return key;
  if (auto mouse = named_after(text, kMousePrefix, mouse_input_from_name)) return mouse;
  return named_after(text, kPadPrefix, gamepad_button_from_name);
}

std::string binding_text(const InputBinding& binding) {
  return std::visit([](auto one) { return text_of(one); }, binding);
}

void ActionBinding::add(const InputBinding& binding) {
  std::visit([this](auto one) { add_once(list_for(*this, one), one); }, binding);
}

void ActionBinding::remove(const InputBinding& binding) {
  std::visit([this](auto one) { remove_all(list_for(*this, one), one); }, binding);
}

std::vector<InputBinding> ActionBinding::all() const {
  std::vector<InputBinding> out(keys.begin(), keys.end());
  out.insert(out.end(), mouse.begin(), mouse.end());
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
  map.bind_action("interact", {{Key::E}, {GamepadButton::Y}});
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
