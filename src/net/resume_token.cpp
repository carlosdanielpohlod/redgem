#include "net/resume_token.hpp"

#include <array>

#include "net/secure/tls_basics.hpp"
#include "net/wire.hpp"

namespace redgem::net {

namespace {

constexpr const char* kTokenPersonalization = "redgem resume token";
constexpr const char* kHexDigits = "0123456789abcdef";
constexpr unsigned kNibbleBits = 4;
constexpr unsigned kNibbleMask = 0x0F;

std::string hexadecimal(const std::array<unsigned char, kResumeTokenBytes>& bytes) {
  std::string digits;
  digits.reserve(kResumeTokenDigits);
  for (const unsigned char byte : bytes) {
    digits.push_back(kHexDigits[byte >> kNibbleBits]);
    digits.push_back(kHexDigits[byte & kNibbleMask]);
  }
  return digits;
}

}  // namespace

std::string new_resume_token() {
  TlsRandom random;
  std::array<unsigned char, kResumeTokenBytes> bytes{};
  if (random.seed(kTokenPersonalization) != 0) return {};
  if (TlsRandom::generate(&random, bytes.data(), bytes.size()) != 0) return {};
  return hexadecimal(bytes);
}

bool same_resume_token(const std::string& offered, const std::string& held) {
  if (held.empty() || offered.size() != held.size()) return false;
  unsigned char difference = 0;
  for (std::size_t i = 0; i < held.size(); ++i) {
    difference |= static_cast<unsigned char>(offered[i] ^ held[i]);
  }
  return difference == 0;
}

}  // namespace redgem::net
