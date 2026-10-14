#include "core/names.hpp"

#include <cctype>
#include <cstdint>
#include <cstdio>

namespace redgem {

namespace {

constexpr std::uint32_t kHashSeed = 2166136261u;  // FNV-1a
constexpr std::uint32_t kHashPrime = 16777619u;
constexpr const char* kHashFormat = "%08x";  // eight hex digits

}  // namespace

bool small_letter_or_digit(unsigned char letter) { return std::islower(letter) != 0 || std::isdigit(letter) != 0; }

std::string name_digits(std::string_view text) {
  std::uint32_t hash = kHashSeed;
  for (const char c : text) hash = (hash ^ static_cast<unsigned char>(c)) * kHashPrime;
  char digits[sizeof("ffffffff")];
  std::snprintf(digits, sizeof(digits), kHashFormat, hash);
  return digits;
}

}  // namespace redgem
