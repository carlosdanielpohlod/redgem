#include "editor/object_clipboard.hpp"

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

namespace redgem::object_clipboard {
namespace {

// The first line of the clipboard's text: what follows is RedGem objects, in this layout.
constexpr const char* kHeader = "RedGem objects 1";
constexpr char kLineEnd = '\n';
constexpr std::string_view kSpaces = " \t\r\n";  // what a clipboard may add to a text (Windows ends lines with \r\n)
constexpr const char* kAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
constexpr char kPadding = '=';
constexpr int kBitsPerDigit = 6;  // what one base64 character holds
constexpr int kBitsPerByte = 8;
constexpr std::uint32_t kDigitMask = 0x3F;
constexpr std::uint32_t kByteMask = 0xFF;
constexpr std::size_t kGroupBytes = 3;  // three bytes make four characters
constexpr std::size_t kGroupDigits = 4;
constexpr int kNotADigit = -1;
constexpr std::size_t kAlphabetSize = 64;
constexpr std::size_t kCharacters = 256;

// The value of each character in base64, kNotADigit for the others.
std::array<int, kCharacters> digit_values() {
  std::array<int, kCharacters> values{};
  values.fill(kNotADigit);
  for (std::size_t i = 0; i < kAlphabetSize; ++i)
    values[static_cast<unsigned char>(kAlphabet[i])] = static_cast<int>(i);
  return values;
}

// Up to three bytes from `start` as one number, the first in the highest bits.
std::uint32_t group_at(const std::vector<std::uint8_t>& bytes, std::size_t start) {
  std::uint32_t group = 0;
  for (std::size_t i = 0; i < kGroupBytes; ++i) {
    const std::uint32_t byte = start + i < bytes.size() ? bytes[start + i] : 0;
    group = (group << kBitsPerByte) | byte;
  }
  return group;
}

// The four characters of a group, padded where the bytes ran out (`present` of three are real).
void append_group(std::string& out, std::uint32_t group, std::size_t present) {
  for (std::size_t digit = 0; digit < kGroupDigits; ++digit) {
    const int shift = kBitsPerDigit * static_cast<int>(kGroupDigits - 1 - digit);
    out += digit <= present ? kAlphabet[(group >> shift) & kDigitMask] : kPadding;
  }
}

// Takes base64 digits one at a time and gives the bytes they make.
struct Decoder {
  std::vector<std::uint8_t> out;
  std::uint32_t pending = 0;  // bits not made into a byte yet, the newest lowest
  int pending_bits = 0;

  void feed(std::uint32_t digit) {
    pending = (pending << kBitsPerDigit) | digit;
    pending_bits += kBitsPerDigit;
    if (pending_bits < kBitsPerByte) return;
    pending_bits -= kBitsPerByte;
    out.push_back(static_cast<std::uint8_t>((pending >> pending_bits) & kByteMask));
  }
};

// Whether the text's first `length` characters end with padding.
bool padding_before(const std::string& text, std::size_t length) { return length > 0 && text[length - 1] == kPadding; }

// The text's characters without the padding at its end.
std::size_t unpadded_length(const std::string& text) {
  std::size_t length = text.size();
  while (padding_before(text, length)) --length;
  return length;
}

bool well_formed(const std::string& text) {
  const std::size_t padding = text.size() - unpadded_length(text);
  return text.size() % kGroupDigits == 0 && padding < kGroupDigits - 1;
}

// The text without spaces and line ends.
std::string without_spaces(const std::string& text) {
  std::string out;
  for (const char c : text) {
    if (kSpaces.find(c) == std::string_view::npos) out += c;
  }
  return out;
}

// The first line of the text, without what ends it.
std::string first_line(const std::string& text) {
  const std::string line = text.substr(0, text.find(kLineEnd));
  return line.substr(0, line.find_last_not_of(kSpaces) + 1);
}

// The text starts with the line that says it holds objects (and has a line after it).
bool has_header(const std::string& text) {
  return text.find(kLineEnd) != std::string::npos && first_line(text) == kHeader;
}

}  // namespace

bool fits_system_clipboard(std::size_t bytes) { return bytes <= kMaxClipboardBytes; }

std::string base64_encode(const std::vector<std::uint8_t>& bytes) {
  std::string out;
  out.reserve((bytes.size() + kGroupBytes - 1) / kGroupBytes * kGroupDigits);
  for (std::size_t start = 0; start < bytes.size(); start += kGroupBytes) {
    append_group(out, group_at(bytes, start), std::min(kGroupBytes, bytes.size() - start));
  }
  return out;
}

std::optional<std::vector<std::uint8_t>> base64_decode(const std::string& text) {
  static const std::array<int, kCharacters> values = digit_values();
  if (!well_formed(text)) return std::nullopt;
  Decoder decoder;
  for (std::size_t i = 0; i < unpadded_length(text); ++i) {
    const int value = values[static_cast<unsigned char>(text[i])];
    if (value == kNotADigit) return std::nullopt;
    decoder.feed(static_cast<std::uint32_t>(value));
  }
  return std::move(decoder.out);
}

std::string to_text(const std::vector<std::uint8_t>& glb) { return kHeader + (kLineEnd + base64_encode(glb)); }

std::optional<std::vector<std::uint8_t>> from_text(const std::string& text) {
  if (!has_header(text)) return std::nullopt;
  return base64_decode(without_spaces(text.substr(text.find(kLineEnd) + 1)));
}

}  // namespace redgem::object_clipboard
