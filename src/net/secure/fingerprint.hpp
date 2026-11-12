#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <utility>

// How a machine that calls another tells whether the one that answered is the one it meant: by
// the SHA-256 of the certificate it answers with (its fingerprint), which a game knows beforehand.
namespace redgem::net {

// A fingerprint in hexadecimal digits (SHA-256: 32 bytes), and the part of it a player-hosted
// game shows as its code (40 bits: forging a certificate that shares them takes far longer than
// a join waits).
inline constexpr std::size_t kFingerprintDigits = 64;
inline constexpr std::size_t kHostCodeDigits = 10;

// The fingerprint of a certificate (its DER bytes): kFingerprintDigits digits, capital letters.
std::string fingerprint_of(const unsigned char* der, std::size_t size);
// As people read it: pairs of digits between colons ("3F:A2:...").
std::string readable_fingerprint(const std::string& digits);
// A player-hosted game's code: the first kHostCodeDigits of its fingerprint, in two groups ("3FA2C-1B09E").
std::string host_code(const std::string& digits);
// What a person gave as a fingerprint or a host's code (any case, with or without ':', '-' or
// spaces), as the digits it stands for; nullopt when it is neither.
std::optional<std::string> pin_digits(const std::string& text);

// How a machine that calls another checks who answered.
class TlsTrust {
 public:
  // No TLS at all: the engine's own tests of what travels, which write it byte by byte. Neither
  // games (Network) nor the server programs ever use it.
  static TlsTrust plaintext() { return TlsTrust(Mode::Plaintext, {}); }
  // Encrypted, believing whoever answers: safe from those who only listen, not from one who
  // answers in the other's place.
  static TlsTrust unverified() { return TlsTrust(Mode::Unverified, {}); }
  // Encrypted, with the one that answers proving it holds the certificate of that fingerprint, or
  // of a fingerprint starting with that host code (`digits` as pin_digits gives them).
  static TlsTrust pinned(std::string digits) { return TlsTrust(Mode::Pinned, std::move(digits)); }

  bool encrypted() const { return mode_ != Mode::Plaintext; }
  bool verifies() const { return mode_ == Mode::Pinned; }
  // Whether a machine that answered with a certificate of this fingerprint is believed.
  bool accepts(const std::string& fingerprint) const;
  const std::string& pin() const { return pin_; }

 private:
  enum class Mode { Plaintext, Unverified, Pinned };
  TlsTrust(Mode mode, std::string pin) : mode_(mode), pin_(std::move(pin)) {}

  Mode mode_;
  std::string pin_;
};

}  // namespace redgem::net
