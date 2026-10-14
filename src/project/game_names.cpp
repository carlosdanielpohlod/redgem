// New games: the names they may be given, and the ids made from them.

#include "project/game_templates.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <random>

namespace redgem {
namespace {

constexpr std::size_t kMaxGameNameLength = 64;
// A new game's id.
constexpr char kIdJoiner = '-';
constexpr const char* kUnnamedId = "game";  // for a name with no letter or digit to make one from
constexpr const char* kIdDigitsFormat = "%08x";

bool ends_with_joiner(const std::string& slug) { return !slug.empty() && slug.back() == kIdJoiner; }

// Whether a dash goes next: after letters, and once however much lies between them and the next.
bool takes_joiner(const std::string& slug) { return !slug.empty() && slug.back() != kIdJoiner; }

// Whether a name is as long as a folder's may be, and starts with something to see.
bool sized_as_name(const std::string& name) {
  return !name.empty() && name.size() <= kMaxGameNameLength && name.front() != ' ';
}

bool name_letter(unsigned char c) { return std::isalnum(c) != 0 || c == '_' || c == '-' || c == ' '; }

// A name in small letters and digits, with one dash for whatever else was between them.
std::string slug_of(const std::string& name) {
  std::string slug;
  for (const char c : name) {
    const auto letter = static_cast<unsigned char>(c);
    if (std::isalnum(letter) != 0) {
      slug.push_back(static_cast<char>(std::tolower(letter)));
    } else if (takes_joiner(slug)) {
      slug.push_back(kIdJoiner);
    }
  }
  if (ends_with_joiner(slug)) slug.pop_back();
  return slug.empty() ? kUnnamedId : slug;
}

}  // namespace

bool valid_game_name(const std::string& name) {
  return sized_as_name(name) && std::all_of(name.begin(), name.end(), name_letter);
}

std::string new_game_id(const std::string& name) {
  char digits[sizeof("ffffffff")];
  std::snprintf(digits, sizeof(digits), kIdDigitsFormat, static_cast<std::uint32_t>(std::random_device{}()));
  return slug_of(name) + kIdJoiner + digits;
}

}  // namespace redgem
