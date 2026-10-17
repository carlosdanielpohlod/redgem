#include "core/filled_texts.hpp"

#include <cstdio>

#include "core/translations.hpp"

namespace redgem {
namespace {

// The values whose texts are kept at most: past them (a frame rate, a count that keeps growing)
// they are all let go, and filled again as they come.
constexpr std::size_t kMostKept = 256;
constexpr std::size_t kNumberDigits = 24;  // room for any long long, with its sign
constexpr const char* kNumberFormat = "%lld";

}  // namespace

const char* FilledTexts::with(std::string_view value) {
  auto text = texts_.find(value);
  if (text != texts_.end()) return text->second.c_str();
  if (texts_.size() >= kMostKept) texts_.clear();
  text = texts_.emplace(std::string(value), tr(key_, {{name_, std::string(value)}})).first;
  return text->second.c_str();
}

const char* FilledTexts::with(long long number) {
  char digits[kNumberDigits];
  std::snprintf(digits, sizeof(digits), kNumberFormat, number);
  return with(std::string_view(digits));
}

}  // namespace redgem
