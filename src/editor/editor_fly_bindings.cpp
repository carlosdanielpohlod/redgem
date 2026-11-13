#include "editor/editor_fly_bindings.hpp"

#include <algorithm>
#include <cctype>

#include "core/translations.hpp"

namespace redgem {
namespace {

bool is_letter(Key key) { return key >= Key::A && key <= Key::Z; }

bool is_arrow(Key key) { return key >= Key::Right && key <= Key::Up; }

FlyDirection direction_using(const FlyBindings& bindings, Key key) {
  const auto found = std::find_if(kFlyBindingFields.begin(), kFlyBindingFields.end(),
                                  [&](const auto& field) { return bindings.*field.direction == key; });
  return found == kFlyBindingFields.end() ? nullptr : found->direction;
}

}  // namespace

bool can_bind_fly_key(Key key) { return is_letter(key) || is_arrow(key); }

bool valid_fly_bindings(const FlyBindings& bindings) {
  for (const auto& field : kFlyBindingFields) {
    const Key key = bindings.*field.direction;
    if (!can_bind_fly_key(key)) return false;
    if (direction_using(bindings, key) != field.direction) return false;
  }
  return true;
}

bool same_fly_bindings(const FlyBindings& first, const FlyBindings& second) {
  return std::all_of(kFlyBindingFields.begin(), kFlyBindingFields.end(), [&](const auto& field) {
    return first.*field.direction == second.*field.direction;
  });
}

void rebind_fly_key(FlyBindings& bindings, FlyDirection direction, Key key) {
  if (!can_bind_fly_key(key)) return;
  if (const auto other = direction_using(bindings, key)) bindings.*other = bindings.*direction;
  bindings.*direction = key;
}

std::string fly_key_label(Key key) {
  switch (key) {
    case Key::Right: return tr("editor.preferences.key_right");
    case Key::Left: return tr("editor.preferences.key_left");
    case Key::Down: return tr("editor.preferences.key_down");
    case Key::Up: return tr("editor.preferences.key_up");
    default: break;
  }
  auto name = key_name(key);
  if (!name.empty()) name.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(name.front())));
  return name;
}

std::string fly_binding_labels(const FlyBindings& bindings) {
  std::string labels;
  for (const auto& field : kFlyBindingFields) {
    if (!labels.empty()) labels += ", ";
    labels += fly_key_label(bindings.*field.direction);
  }
  return labels;
}

}  // namespace redgem
