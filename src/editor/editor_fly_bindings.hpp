#pragma once

#include <array>
#include <string>

#include "input/keys.hpp"

namespace redgem {

// These keys are read only by the editor's camera while the right mouse button is held.
struct FlyBindings {
  Key forward = Key::W;
  Key back = Key::S;
  Key left = Key::A;
  Key right = Key::D;
  Key up = Key::E;
  Key down = Key::Q;
};

using FlyDirection = Key FlyBindings::*;

struct FlyBindingField {
  FlyDirection direction;
  const char* setting;
  const char* label;
};

inline constexpr std::array kFlyBindingFields{
    FlyBindingField{&FlyBindings::forward, "forward", "editor.preferences.fly_forward"},
    FlyBindingField{&FlyBindings::back, "back", "editor.preferences.fly_back"},
    FlyBindingField{&FlyBindings::left, "left", "editor.preferences.fly_left"},
    FlyBindingField{&FlyBindings::right, "right", "editor.preferences.fly_right"},
    FlyBindingField{&FlyBindings::up, "up", "editor.preferences.fly_up"},
    FlyBindingField{&FlyBindings::down, "down", "editor.preferences.fly_down"},
};

bool can_bind_fly_key(Key key);
bool valid_fly_bindings(const FlyBindings& bindings);
bool same_fly_bindings(const FlyBindings& first, const FlyBindings& second);

// A key already assigned to another direction trades places with this direction's old key.
void rebind_fly_key(FlyBindings& bindings, FlyDirection direction, Key key);
std::string fly_key_label(Key key);
std::string fly_binding_labels(const FlyBindings& bindings);

}  // namespace redgem
