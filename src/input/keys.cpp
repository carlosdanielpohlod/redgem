#include "input/keys.hpp"

#include <map>

namespace redgem {
namespace {

constexpr int kFirstLetter = static_cast<int>(Key::A);
constexpr int kLastLetter = static_cast<int>(Key::Z);
constexpr int kFirstDigit = static_cast<int>(Key::Zero);
constexpr int kLastDigit = static_cast<int>(Key::Nine);

const std::map<std::string, Key>& key_names() {
  static const std::map<std::string, Key> names = [] {
    std::map<std::string, Key> table = {
        {"space", Key::Space},
        {"apostrophe", Key::Apostrophe},
        {"comma", Key::Comma},
        {"minus", Key::Minus},
        {"period", Key::Period},
        {"slash", Key::Slash},
        {"escape", Key::Escape},
        {"enter", Key::Enter},
        {"tab", Key::Tab},
        {"backspace", Key::Backspace},
        {"right", Key::Right},
        {"left", Key::Left},
        {"down", Key::Down},
        {"up", Key::Up},
        {"left_shift", Key::LeftShift},
        {"left_control", Key::LeftControl},
        {"left_alt", Key::LeftAlt},
        {"right_shift", Key::RightShift},
        {"right_control", Key::RightControl},
    };
    for (int code = kFirstLetter; code <= kLastLetter; ++code) {
      table.emplace(std::string(1, static_cast<char>('a' + (code - kFirstLetter))), static_cast<Key>(code));
    }
    for (int code = kFirstDigit; code <= kLastDigit; ++code) {
      table.emplace(std::string(1, static_cast<char>('0' + (code - kFirstDigit))), static_cast<Key>(code));
    }
    return table;
  }();
  return names;
}

}  // namespace

std::optional<Key> key_from_name(const std::string& name) {
  const auto it = key_names().find(name);
  if (it == key_names().end()) return std::nullopt;
  return it->second;
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
