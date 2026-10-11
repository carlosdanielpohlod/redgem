#include "scene/extras_parser.hpp"

#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <stdexcept>

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
constexpr std::string_view kTrueLiteral = "true";
constexpr std::string_view kFalseLiteral = "false";
constexpr std::string_view kNullLiteral = "null";
// What a JSON number is written with. (No letters but e: "inf", "nan" and hexadecimal are not numbers here.)
constexpr std::string_view kNumberCharacters = "+-0123456789.eE";
// The longest number read, in characters: more digits than a double can tell apart, with room to spare.
constexpr std::size_t kMaxNumberLength = 64;
// How deep the arrays and objects of a value that is skipped may be nested: each level takes stack.
constexpr int kMaxNesting = 64;

// The one-letter escapes of a JSON string: what follows the backslash, and what it stands for.
struct Escape {
  char written;
  char meaning;
};
constexpr Escape kEscapes[] = {{'"', '"'},  {'\\', '\\'}, {'/', '/'},  {'b', '\b'},
                               {'f', '\f'}, {'n', '\n'},  {'r', '\r'}, {'t', '\t'}};

class ParseError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

// Minimal recursive-descent JSON reader: enough for Blender custom properties.
class JsonReader {
 public:
  explicit JsonReader(std::string_view text) : text_(text) {}

  void parse_object(ExtrasParseResult& result) {
    skip_whitespace();
    expect('{');
    skip_whitespace();
    if (!try_consume('}')) read_members(result);
    finish();
  }

 private:
  // The members of an object whose '{' was read, up to and including its '}'.
  void read_members(ExtrasParseResult& result) {
    do {
      const std::string key = parse_key();
      read_member(key, result);
      skip_whitespace();
    } while (try_consume(','));
    expect('}');
  }

  // A member's name and the ':' after it.
  std::string parse_key() {
    skip_whitespace();
    std::string key = parse_string();
    skip_whitespace();
    expect(':');
    skip_whitespace();
    return key;
  }
  void read_member(const std::string& key, ExtrasParseResult& result) {
    const char c = peek();
    if (c == '"') {
      result.properties[key] = parse_string();
    } else if (c == 't' || c == 'f') {
      result.properties[key] = parse_bool();
    } else if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
      result.properties[key] = parse_number();
    } else {
      skip_value();
      result.skipped_keys.push_back(key);
    }
  }

  void finish() {
    skip_whitespace();
    if (pos_ != text_.size()) fail("trailing characters after object");
  }

  std::string parse_string() {
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

  // What follows a backslash in a string.
  void append_escape(std::string& out) {
    const char escaped = next();
    if (escaped == 'u') {
      append_utf8(out, parse_hex4());
    } else {
      out.push_back(unescaped(escaped));
    }
  }

  char unescaped(char escaped) const {
    for (const Escape& known : kEscapes) {
      if (known.written == escaped) return known.meaning;
    }
    fail("invalid escape sequence");
  }

  unsigned parse_hex4() {
    if (pos_ + kUnicodeEscapeDigits > text_.size()) fail("truncated unicode escape");
    const std::string digits(text_.substr(pos_, kUnicodeEscapeDigits));
    pos_ += kUnicodeEscapeDigits;
    char* end = nullptr;
    const unsigned long value = std::strtoul(digits.c_str(), &end, kHexBase);
    if (end != digits.c_str() + kUnicodeEscapeDigits) fail("invalid unicode escape");
    return static_cast<unsigned>(value);
  }

  static void append_utf8(std::string& out, unsigned code) {
    if (code < kOneByteLimit) {
      out.push_back(static_cast<char>(code));
    } else if (code < kTwoByteLimit) {
      out.push_back(static_cast<char>(kTwoByteLead | (code >> kSixBits)));
      out.push_back(static_cast<char>(kContinuation | (code & kSixBitMask)));
    } else {
      out.push_back(static_cast<char>(kThreeByteLead | (code >> kTwelveBits)));
      out.push_back(static_cast<char>(kContinuation | ((code >> kSixBits) & kSixBitMask)));
      out.push_back(static_cast<char>(kContinuation | (code & kSixBitMask)));
    }
  }

  bool parse_bool() {
    if (consume_literal(kTrueLiteral)) return true;
    if (consume_literal(kFalseLiteral)) return false;
    fail("invalid literal");
  }

  // The characters of the number starting here. (Only they are copied: the rest of the text can
  // be long, and there can be many numbers in it.)
  std::string_view number_text() const {
    const std::size_t end = text_.find_first_not_of(kNumberCharacters, pos_);
    const std::string_view written = text_.substr(pos_, end == std::string_view::npos ? end : end - pos_);
    if (written.empty() || written.size() > kMaxNumberLength) fail("invalid number");
    return written;
  }

  double parse_number() {
    const std::string_view written = number_text();
    std::array<char, kMaxNumberLength + 1> digits{};  // zeros: the copy ends where the number does
    written.copy(digits.data(), written.size());
    char* end = nullptr;
    const double value = std::strtod(digits.data(), &end);
    if (end != digits.data() + written.size() || !std::isfinite(value)) fail("invalid number");
    pos_ += written.size();
    return value;
  }

  // Skips arrays, objects and null without interpreting them.
  void skip_value() {
    const char c = peek();
    if (c == '"') {
      parse_string();
    } else if (c == '[' || c == '{') {
      skip_container(c);
    } else if (!consume_any_literal()) {
      parse_number();
    }
  }

  // An array or an object, from its opening bracket to its closing one.
  void skip_container(char open) {
    const char close = open == '[' ? ']' : '}';
    if (++nesting_ > kMaxNesting) fail("nested too deeply");
    ++pos_;
    skip_whitespace();
    if (!try_consume(close)) skip_elements(open, close);
    --nesting_;
  }

  // What a container whose opening bracket was read holds, up to and including its closing one.
  void skip_elements(char open, char close) {
    do {
      skip_whitespace();
      if (open == '{') parse_key();
      skip_value();
      skip_whitespace();
    } while (try_consume(','));
    expect(close);
  }

  bool consume_any_literal() {
    return consume_literal(kNullLiteral) || consume_literal(kTrueLiteral) || consume_literal(kFalseLiteral);
  }

  bool consume_literal(std::string_view literal) {
    if (text_.substr(pos_, literal.size()) != literal) return false;
    pos_ += literal.size();
    return true;
  }

  void skip_whitespace() {
    while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) ++pos_;
  }
  char peek() const {
    if (pos_ >= text_.size()) fail("unexpected end of input");
    return text_[pos_];
  }
  char next() {
    const char c = peek();
    ++pos_;
    return c;
  }
  bool try_consume(char c) {
    if (pos_ < text_.size() && text_[pos_] == c) {
      ++pos_;
      return true;
    }
    return false;
  }
  void expect(char c) {
    if (!try_consume(c)) fail(std::string("expected '") + c + "'");
  }
  [[noreturn]] void fail(const std::string& message) const {
    throw ParseError(message + " at offset " + std::to_string(pos_));
  }

  std::string_view text_;
  std::size_t pos_ = 0;
  int nesting_ = 0;  // how many arrays and objects being skipped are open
};

bool is_blank(std::string_view text) {
  for (const char c : text) {
    if (!std::isspace(static_cast<unsigned char>(c))) return false;
  }
  return true;
}

}  // namespace

ExtrasParseResult parse_extras(std::string_view json) {
  ExtrasParseResult result;
  if (is_blank(json)) return result;
  try {
    JsonReader(json).parse_object(result);
  } catch (const ParseError& error) {
    result = ExtrasParseResult{};
    result.ok = false;
    result.error = error.what();
  }
  return result;
}

}  // namespace redgem
