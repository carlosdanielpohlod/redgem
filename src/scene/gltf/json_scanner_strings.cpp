// The texts in quotes of a JSON text, and what their escapes stand for.

#include <cstdlib>

#include "scene/gltf/json_scanner.hpp"

namespace redgem {
namespace {

constexpr int kHexBase = 16;
constexpr int kUnicodeEscapeDigits = 4;
constexpr unsigned kOneByteLimit = 0x80;
constexpr unsigned kTwoByteLimit = 0x800;
constexpr unsigned kTwoByteLead = 0xC0;
constexpr unsigned kThreeByteLead = 0xE0;
constexpr unsigned kContinuation = 0x80;
constexpr unsigned kSixBitMask = 0x3F;
constexpr int kSixBits = 6;
constexpr int kTwelveBits = 12;

// The one-letter escapes of a JSON string: what follows the backslash, and what it stands for.
struct Escape {
  char written;
  char meaning;
};
constexpr Escape kEscapes[] = {{'"', '"'},  {'\\', '\\'}, {'/', '/'},  {'b', '\b'},
                               {'f', '\f'}, {'n', '\n'},  {'r', '\r'}, {'t', '\t'}};

// What a one-letter escape stands for; null for a letter that is not one.
const char* unescaped(char escaped) {
  for (const Escape& known : kEscapes) {
    if (known.written == escaped) return &known.meaning;
  }
  return nullptr;
}

char continuation_byte(unsigned bits) { return static_cast<char>(kContinuation | (bits & kSixBitMask)); }

void append_utf8(std::string& out, unsigned code) {
  if (code < kOneByteLimit) {
    out.push_back(static_cast<char>(code));
  } else if (code < kTwoByteLimit) {
    out.push_back(static_cast<char>(kTwoByteLead | (code >> kSixBits)));
    out.push_back(continuation_byte(code));
  } else {
    out.push_back(static_cast<char>(kThreeByteLead | (code >> kTwelveBits)));
    out.push_back(continuation_byte(code >> kSixBits));
    out.push_back(continuation_byte(code));
  }
}

}  // namespace

std::string JsonScanner::read_string() {
  expect('"');
  std::string out;
  for (char c = next(); c != '"'; c = next()) {
    if (c == '\\') {
      append_escape(out);
    } else {
      out.push_back(c);
    }
  }
  return out;
}

void JsonScanner::append_escape(std::string& out) {
  const char escaped = next();
  if (escaped == 'u') return append_utf8(out, read_hex4());
  const char* meaning = unescaped(escaped);
  if (meaning == nullptr) fail("invalid escape sequence");
  out.push_back(*meaning);
}

unsigned JsonScanner::read_hex4() {
  if (pos_ + kUnicodeEscapeDigits > text_.size()) fail("truncated unicode escape");
  const std::string digits(text_.substr(pos_, kUnicodeEscapeDigits));
  pos_ += kUnicodeEscapeDigits;
  char* end = nullptr;
  const unsigned long value = std::strtoul(digits.c_str(), &end, kHexBase);
  if (end != digits.c_str() + kUnicodeEscapeDigits) fail("invalid unicode escape");
  return static_cast<unsigned>(value);
}

}  // namespace redgem
