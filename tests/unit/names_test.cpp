#include <doctest/doctest.h>

#include <string>

#include "core/names.hpp"

using namespace redgem;

namespace {

constexpr std::size_t kDigits = 8;
constexpr const char* kHexDigits = "0123456789abcdef";

}  // namespace

TEST_CASE("an id's letters are the small ones and the digits") {
  CHECK(small_letter_or_digit('a'));
  CHECK(small_letter_or_digit('z'));
  CHECK(small_letter_or_digit('7'));
  CHECK_FALSE(small_letter_or_digit('A'));
  CHECK_FALSE(small_letter_or_digit('-'));
  CHECK_FALSE(small_letter_or_digit(' '));
}

TEST_CASE("a name's digits are eight hex digits, the same every time and other ones for another name") {
  const std::string digits = name_digits("My Game");
  CHECK(digits.size() == kDigits);
  CHECK(digits.find_first_not_of(kHexDigits) == std::string::npos);
  CHECK(name_digits("My Game") == digits);
  CHECK(name_digits("my-game") != digits);
}

TEST_CASE("the digits of a name are the ones every machine makes of it") {
  // (FNV-1a's own numbers: a save or an app found by them is found again by any build.)
  CHECK(name_digits("") == "811c9dc5");
  CHECK(name_digits("a") == "e40c292c");
}
