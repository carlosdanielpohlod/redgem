#include "scene/gltf/json_scanner.hpp"

#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace redgem {
namespace {

constexpr std::string_view kTrueLiteral = "true";
constexpr std::string_view kFalseLiteral = "false";
constexpr std::string_view kNullLiteral = "null";
// What a JSON number is written with. (No letters but e: "inf", "nan" and hexadecimal are not numbers here.)
constexpr std::string_view kNumberCharacters = "+-0123456789.eE";
// The longest number read, in characters: more digits than a double can tell apart, with room to spare.
constexpr std::size_t kMaxNumberLength = 64;

}  // namespace

char JsonScanner::peek() const {
  if (at_end()) fail("unexpected end of input");
  return text_[pos_];
}

char JsonScanner::next() {
  const char c = peek();
  ++pos_;
  return c;
}

bool JsonScanner::try_consume(char c) {
  const bool is_next = !at_end() && text_[pos_] == c;
  if (is_next) ++pos_;
  return is_next;
}

void JsonScanner::expect(char c) {
  if (!try_consume(c)) fail(std::string("expected '") + c + "'");
}

bool JsonScanner::at_whitespace() const {
  return !at_end() && std::isspace(static_cast<unsigned char>(text_[pos_])) != 0;
}

void JsonScanner::skip_whitespace() {
  while (at_whitespace()) ++pos_;
}

bool JsonScanner::consume_literal(std::string_view literal) {
  if (text_.substr(pos_, literal.size()) != literal) return false;
  pos_ += literal.size();
  return true;
}

bool JsonScanner::consume_any_literal() {
  return consume_literal(kNullLiteral) || consume_literal(kTrueLiteral) || consume_literal(kFalseLiteral);
}

void JsonScanner::fail(const std::string& message) const {
  throw JsonError(message + " at offset " + std::to_string(pos_));
}

bool JsonScanner::read_bool() {
  if (consume_literal(kTrueLiteral)) return true;
  if (consume_literal(kFalseLiteral)) return false;
  fail("invalid literal");
}

// (Only they are copied: the rest of the text can be long, and there can be many numbers in it.)
std::string_view JsonScanner::number_text() const {
  const std::size_t end = text_.find_first_not_of(kNumberCharacters, pos_);
  const std::string_view written = text_.substr(pos_, end == std::string_view::npos ? end : end - pos_);
  const bool readable = !written.empty() && written.size() <= kMaxNumberLength;
  if (!readable) fail("invalid number");
  return written;
}

double JsonScanner::read_number() {
  const std::string_view written = number_text();
  std::array<char, kMaxNumberLength + 1> digits{};  // zeros: the copy ends where the number does
  written.copy(digits.data(), written.size());
  char* end = nullptr;
  const double value = std::strtod(digits.data(), &end);
  const bool is_a_number = end == digits.data() + written.size() && std::isfinite(value);
  if (!is_a_number) fail("invalid number");
  pos_ += written.size();
  return value;
}

}  // namespace redgem
