#include "input/keys.hpp"

#include <map>

namespace redgem {
namespace {

using KeyTable = std::map<std::string, Key>;

void add_symbol_keys(KeyTable& table) {
  table.insert({{"space", Key::Space},
                {"apostrophe", Key::Apostrophe},
                {"comma", Key::Comma},
                {"minus", Key::Minus},
                {"period", Key::Period},
                {"slash", Key::Slash}});
}

void add_editing_keys(KeyTable& table) {
  table.insert({{"escape", Key::Escape}, {"enter", Key::Enter}, {"tab", Key::Tab}, {"backspace", Key::Backspace}});
}

void add_arrow_keys(KeyTable& table) {
  table.insert({{"right", Key::Right}, {"left", Key::Left}, {"down", Key::Down}, {"up", Key::Up}});
}

void add_modifier_keys(KeyTable& table) {
  table.insert({{"left_shift", Key::LeftShift},
                {"left_control", Key::LeftControl},
                {"left_alt", Key::LeftAlt},
                {"right_shift", Key::RightShift},
                {"right_control", Key::RightControl}});
}

// Keys whose codes follow one another, named by characters that do too: "a" to "z", "0" to "9".
void add_key_run(KeyTable& table, Key first, Key last, char first_name) {
  const int first_code = static_cast<int>(first);
  for (int code = first_code; code <= static_cast<int>(last); ++code) {
    table.emplace(std::string(1, static_cast<char>(first_name + (code - first_code))), static_cast<Key>(code));
  }
}

KeyTable named_keys() {
  KeyTable table;
  add_symbol_keys(table);
  add_editing_keys(table);
  add_arrow_keys(table);
  add_modifier_keys(table);
  add_key_run(table, Key::A, Key::Z, 'a');
  add_key_run(table, Key::Zero, Key::Nine, '0');
  return table;
}

const KeyTable& key_names() {
  static const KeyTable names = named_keys();
  return names;
}

}  // namespace

std::optional<Key> key_from_name(const std::string& name) {
  const auto it = key_names().find(name);
  if (it == key_names().end()) return std::nullopt;
  return it->second;
}

std::string key_name(Key key) {
  for (const auto& [name, named] : key_names()) {
    if (named == key) return name;
  }
  return {};
}

const std::vector<Key>& all_keys() {
  static const std::vector<Key> keys = [] {
    std::vector<Key> out;
    for (const auto& [name, key] : key_names()) out.push_back(key);
    return out;
  }();
  return keys;
}

}  // namespace redgem
