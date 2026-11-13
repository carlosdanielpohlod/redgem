#include "net/secure/fingerprint.hpp"

#include <mbedtls/sha256.h>

#include <algorithm>
#include <cctype>
#include <string_view>

namespace redgem::net {

namespace {

constexpr std::size_t kDigestBytes = 32;
constexpr int kIsSha256 = 0;  // (mbedtls_sha256's last argument: not SHA-224)
constexpr std::string_view kHexDigits = "0123456789ABCDEF";
constexpr unsigned kNibbleBits = 4;
constexpr unsigned kNibbleMask = 0x0F;
constexpr std::size_t kDigitsPerByte = 2;
constexpr char kByteSeparator = ':';
constexpr char kCodeSeparator = '-';
constexpr std::size_t kCodeGroupDigits = kHostCodeDigits / 2;
// What a person may write between the digits: nothing of the fingerprint.
constexpr std::string_view kSeparators = ":- ";

bool is_separator(char letter) { return kSeparators.find(letter) != std::string_view::npos; }

bool is_hex_digit(char letter) { return std::isxdigit(static_cast<unsigned char>(letter)) != 0; }

// Whether so many digits make a pin: a whole fingerprint, or a host's code.
bool pin_length(std::size_t digits) { return digits == kFingerprintDigits || digits == kHostCodeDigits; }

// Whether a fingerprint starts with the pin. (A pin is never empty: one would match every certificate.)
bool matches_pin(const std::string& fingerprint, const std::string& pin) {
  return !pin.empty() && fingerprint.compare(0, pin.size(), pin) == 0;
}

}  // namespace

std::string fingerprint_of(const unsigned char* der, std::size_t size) {
  unsigned char digest[kDigestBytes] = {};
  mbedtls_sha256(der, size, digest, kIsSha256);
  std::string digits;
  digits.reserve(kFingerprintDigits);
  for (const unsigned char byte : digest) {
    digits += kHexDigits[byte >> kNibbleBits];
    digits += kHexDigits[byte & kNibbleMask];
  }
  return digits;
}

std::string readable_fingerprint(const std::string& digits) {
  std::string readable;
  for (std::size_t at = 0; at < digits.size(); at += kDigitsPerByte) {
    if (at > 0) readable += kByteSeparator;
    readable += digits.substr(at, kDigitsPerByte);
  }
  return readable;
}

std::string host_code(const std::string& digits) {
  return digits.substr(0, kCodeGroupDigits) + kCodeSeparator + digits.substr(kCodeGroupDigits, kCodeGroupDigits);
}

std::optional<std::string> pin_digits(const std::string& text) {
  std::string digits;
  for (const char letter : text) {
    if (is_separator(letter)) continue;
    if (!is_hex_digit(letter)) return std::nullopt;
    digits += static_cast<char>(std::toupper(static_cast<unsigned char>(letter)));
  }
  if (!pin_length(digits.size())) return std::nullopt;
  return digits;
}

bool TlsTrust::accepts(const std::string& fingerprint) const { return !verifies() || matches_pin(fingerprint, pin_); }

}  // namespace redgem::net
