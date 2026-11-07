#include "input/keys.hpp"

#include <map>

namespace redgem {
namespace {

using KeyTable = std::map<std::string, Key>;

// The function keys and the keypad's digits are named by a word and a number: "f1" to "f12",
// "kp_0" to "kp_9".
constexpr const char* kFunctionPrefix = "f";
constexpr const char* kKeypadPrefix = "kp_";
constexpr int kFirstFunctionNumber = 1;
constexpr int kFirstKeypadDigit = 0;

void add_symbol_keys(KeyTable& table) {
  table.insert({{"space", Key::Space},
                {"apostrophe", Key::Apostrophe},
                {"comma", Key::Comma},
                {"minus", Key::Minus},
                {"period", Key::Period},
                {"slash", Key::Slash},
                {"semicolon", Key::Semicolon},
                {"equals", Key::Equals},
                {"left_bracket", Key::LeftBracket},
                {"backslash", Key::Backslash},
                {"right_bracket", Key::RightBracket},
                {"grave", Key::Grave}});
}

void add_editing_keys(KeyTable& table) {
  table.insert({{"escape", Key::Escape},
                {"enter", Key::Enter},
                {"tab", Key::Tab},
                {"backspace", Key::Backspace},
                {"insert", Key::Insert},
                {"delete", Key::Delete},
                {"home", Key::Home},
                {"end", Key::End},
                {"page_up", Key::PageUp},
                {"page_down", Key::PageDown}});
}

void add_arrow_keys(KeyTable& table) {
  table.insert({{"right", Key::Right}, {"left", Key::Left}, {"down", Key::Down}, {"up", Key::Up}});
}

void add_lock_and_system_keys(KeyTable& table) {
  table.insert({{"caps_lock", Key::CapsLock},
                {"scroll_lock", Key::ScrollLock},
                {"num_lock", Key::NumLock},
                {"print_screen", Key::PrintScreen},
                {"pause", Key::Pause},
                {"menu", Key::Menu}});
}

void add_modifier_keys(KeyTable& table) {
  table.insert({{"left_shift", Key::LeftShift},
                {"left_control", Key::LeftControl},
                {"left_alt", Key::LeftAlt},
                {"left_super", Key::LeftSuper},
                {"right_shift", Key::RightShift},
                {"right_control", Key::RightControl},
                {"right_alt", Key::RightAlt},
                {"right_super", Key::RightSuper}});
}

// The keypad's keys apart from its digits.
void add_keypad_keys(KeyTable& table) {
  table.insert({{"kp_decimal", Key::KeypadDecimal},
                {"kp_divide", Key::KeypadDivide},
                {"kp_multiply", Key::KeypadMultiply},
                {"kp_subtract", Key::KeypadSubtract},
                {"kp_add", Key::KeypadAdd},
                {"kp_enter", Key::KeypadEnter},
                {"kp_equals", Key::KeypadEquals}});
}

// Keys whose codes follow one another, named by characters that do too: "a" to "z", "0" to "9".
void add_key_run(KeyTable& table, Key first, Key last, char first_name) {
  const int first_code = static_cast<int>(first);
  for (int code = first_code; code <= static_cast<int>(last); ++code) {
    table.emplace(std::string(1, static_cast<char>(first_name + (code - first_code))), static_cast<Key>(code));
  }
}

// Keys whose codes follow one another, named by a word and numbers that do too: "f1" to "f12".
void add_numbered_run(KeyTable& table, Key first, Key last, const char* prefix, int first_number) {
  const int first_code = static_cast<int>(first);
  for (int code = first_code; code <= static_cast<int>(last); ++code) {
    table.emplace(prefix + std::to_string(first_number + (code - first_code)), static_cast<Key>(code));
  }
}

KeyTable named_keys() {
  KeyTable table;
  add_symbol_keys(table);
  add_editing_keys(table);
  add_arrow_keys(table);
  add_lock_and_system_keys(table);
  add_modifier_keys(table);
  add_keypad_keys(table);
  add_key_run(table, Key::A, Key::Z, 'a');
  add_key_run(table, Key::Zero, Key::Nine, '0');
  add_numbered_run(table, Key::F1, Key::F12, kFunctionPrefix, kFirstFunctionNumber);
  add_numbered_run(table, Key::Keypad0, Key::Keypad9, kKeypadPrefix, kFirstKeypadDigit);
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
