#include "core/language_names.hpp"

#include <algorithm>
#include <cctype>

namespace redgem {
namespace {

constexpr char kPartMark = '_';       // between a language and its region in a key
constexpr char kOtherPartMark = '-';  // ... as BCP 47 and Windows write it
using Matches = bool (*)(const std::string& asked, const std::string& candidate);

char key_letter(unsigned char letter) {
  return letter == kOtherPartMark ? kPartMark : static_cast<char>(std::tolower(letter));
}

bool same_name(const std::string& asked, const std::string& candidate) { return candidate == asked; }

bool is_the_base(const std::string& asked, const std::string& candidate) { return candidate == base_language(asked); }

bool same_base(const std::string& asked, const std::string& candidate) {
  return base_language(candidate) == base_language(asked);
}

// The ways a candidate may serve, the best first.
constexpr Matches kMatches[] = {same_name, is_the_base, same_base};

}  // namespace

std::string language_key(std::string_view name) {
  std::string key(name);
  std::transform(key.begin(), key.end(), key.begin(), [](unsigned char letter) { return key_letter(letter); });
  return key;
}

std::string base_language(std::string_view name) {
  const std::string key = language_key(name);
  return key.substr(0, key.find(kPartMark));
}

std::optional<std::string> best_language(std::string_view asked, const std::vector<std::string>& available) {
  const std::string wanted = language_key(asked);
  for (const Matches matches : kMatches) {
    const auto found = std::find_if(available.begin(), available.end(), [&](const std::string& candidate) {
      return matches(wanted, language_key(candidate));
    });
    if (found != available.end()) return *found;
  }
  return std::nullopt;
}

}  // namespace redgem
