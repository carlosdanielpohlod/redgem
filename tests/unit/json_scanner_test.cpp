#include <doctest/doctest.h>

#include <string>

#include "scene/gltf/json_scanner.hpp"

using namespace redgem;

namespace {

// What a scanner says is wrong with a text when `read` is tried on it; empty when nothing is.
template <typename Read>
std::string trouble(const char* text, Read read) {
  JsonScanner scanner(text);
  try {
    read(scanner);
  } catch (const JsonError& error) {
    return error.what();
  }
  return {};
}

}  // namespace

TEST_CASE("a scanner steps through a text, over the spaces and the characters it is told to expect") {
  JsonScanner scanner("  { }");
  scanner.skip_whitespace();
  CHECK(scanner.peek() == '{');
  CHECK_FALSE(scanner.try_consume('}'));
  scanner.expect('{');
  scanner.skip_whitespace();
  CHECK(scanner.next() == '}');
  CHECK(scanner.at_end());
}

TEST_CASE("what a text gets wrong is said with where it is") {
  CHECK(trouble("x", [](JsonScanner& json) { json.expect('{'); }) == "expected '{' at offset 0");
  CHECK(trouble("ab", [](JsonScanner& json) {
          json.next();
          json.next();
          json.peek();
        }) == "unexpected end of input at offset 2");
}

TEST_CASE("texts are read with their escapes decoded, unicode ones as UTF-8") {
  JsonScanner plain(R"("a \"quoted\" \\ line\nnext" rest)");
  CHECK(plain.read_string() == "a \"quoted\" \\ line\nnext");
  CHECK(plain.peek() == ' ');  // it stops after the closing quote

  CHECK(JsonScanner(R"("A")").read_string() == "A");
  CHECK(JsonScanner(R"("é")").read_string() == "\xC3\xA9");      // two bytes
  CHECK(JsonScanner(R"("€")").read_string() == "\xE2\x82\xAC");  // three bytes

  const auto read_text = [](JsonScanner& json) { json.read_string(); };
  CHECK(trouble(R"("\q")", read_text).find("invalid escape sequence") == 0);
  CHECK(trouble(R"("\u12")", read_text).find("truncated unicode escape") == 0);
  CHECK(trouble(R"("\u12zz")", read_text).find("invalid unicode escape") == 0);
  CHECK(trouble(R"("never closed)", read_text).find("unexpected end of input") == 0);
}

TEST_CASE("a number is read up to where it ends, and only when it is one") {
  JsonScanner scanner("-12.5e-1, 3");
  CHECK(scanner.read_number() == doctest::Approx(-1.25));
  CHECK(scanner.peek() == ',');

  const auto read_number = [](JsonScanner& json) { json.read_number(); };
  // Letters are not part of a number: of a hexadecimal one, only the zero is read.
  JsonScanner hexadecimal("0x10");
  CHECK(hexadecimal.read_number() == doctest::Approx(0.0));
  CHECK(hexadecimal.peek() == 'x');

  for (const char* bad : {"1e999", "-", "1e", "x", ""}) {
    CAPTURE(bad);
    CHECK(trouble(bad, read_number).find("invalid number") == 0);
  }
}

TEST_CASE("true, false and null are read as words") {
  CHECK(JsonScanner("true").read_bool());
  CHECK_FALSE(JsonScanner("false").read_bool());
  CHECK(trouble("tru", [](JsonScanner& json) { json.read_bool(); }).find("invalid literal") == 0);

  JsonScanner words("nulltruefalse?");
  CHECK(words.consume_any_literal());
  CHECK(words.consume_any_literal());
  CHECK(words.consume_any_literal());
  CHECK_FALSE(words.consume_any_literal());
  CHECK(words.peek() == '?');
}
