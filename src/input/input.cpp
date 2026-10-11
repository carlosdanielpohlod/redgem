#include "input/input.hpp"

#include <algorithm>

namespace redgem {
namespace {

constexpr float kAxisNegative = -1.0f;
constexpr float kAxisPositive = 1.0f;

}  // namespace

void ScriptedInputSource::set_key(Key key, bool down) {
  if (down) {
    held_.insert(key);
  } else {
    held_.erase(key);
  }
}

InputMap InputMap::defaults() {
  InputMap map;
  map.bind_axis("horizontal", {{Key::A, Key::Left}, {Key::D, Key::Right}});
  map.bind_axis("vertical", {{Key::S, Key::Down}, {Key::W, Key::Up}});
  map.bind_action("jump", {Key::Space});
  map.bind_action("fire", {Key::LeftControl, Key::Enter});
  map.bind_action("run", {Key::LeftShift});
  return map;
}

const AxisBinding* InputMap::axis(const std::string& name) const {
  const auto it = axes_.find(name);
  return it == axes_.end() ? nullptr : &it->second;
}

const std::vector<Key>* InputMap::action(const std::string& name) const {
  const auto it = actions_.find(name);
  return it == actions_.end() ? nullptr : &it->second;
}

void InputState::update(const InputSource& source) {
  previous_ = std::move(current_);
  current_.clear();
  for (const Key key : all_keys()) {
    if (source.is_key_down(key)) current_.insert(key);
  }
  previous_mouse_ = mouse_;
  mouse_ = source.mouse();
}

std::optional<MouseButton> mouse_button_from_name(const std::string& name) {
  if (name == "left") return MouseButton::Left;
  if (name == "right") return MouseButton::Right;
  if (name == "middle") return MouseButton::Middle;
  return std::nullopt;
}

std::optional<float> InputState::axis(const std::string& name) const {
  const AxisBinding* binding = map_.axis(name);
  if (binding == nullptr) return std::nullopt;
  auto any_down = [this](const std::vector<Key>& keys) {
    return std::any_of(keys.begin(), keys.end(), [this](Key key) { return down(key); });
  };
  float value = 0.0f;
  if (any_down(binding->negative)) value += kAxisNegative;
  if (any_down(binding->positive)) value += kAxisPositive;
  return value;
}

std::optional<bool> InputState::action_down(const std::string& name) const {
  const std::vector<Key>* keys = map_.action(name);
  if (keys == nullptr) return std::nullopt;
  return std::any_of(keys->begin(), keys->end(), [this](Key key) { return down(key); });
}

std::optional<bool> InputState::action_pressed(const std::string& name) const {
  const std::vector<Key>* keys = map_.action(name);
  if (keys == nullptr) return std::nullopt;
  return std::any_of(keys->begin(), keys->end(), [this](Key key) { return pressed(key); });
}

}  // namespace redgem
