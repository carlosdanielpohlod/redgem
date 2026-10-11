#include <doctest/doctest.h>

#include <chrono>
#include <string>
#include <vector>

#include "scene/extras_parser.hpp"

using namespace redgem;

TEST_CASE("parses scalar Blender properties") {
  const auto result = parse_extras(R"({"script": "player.rb", "speed": 6.5, "trigger": true, "off": false, "n": -3})");
  REQUIRE(result.ok);
  CHECK(get_string(result.properties, "script") == "player.rb");
  CHECK(get_number(result.properties, "speed") == doctest::Approx(6.5));
  CHECK(get_bool(result.properties, "trigger") == true);
  CHECK(get_bool(result.properties, "off") == false);
  CHECK(get_number(result.properties, "n") == doctest::Approx(-3.0));
}

TEST_CASE("skips arrays, objects and null but reports them") {
  const auto result = parse_extras(R"({"color": [1, 0, 0], "nested": {"a": {"b": [1]}}, "nothing": null, "ok": 1})");
  REQUIRE(result.ok);
  CHECK(result.properties.size() == 1);
  CHECK(result.skipped_keys == std::vector<std::string>{"color", "nested", "nothing"});
}

TEST_CASE("decodes string escapes including unicode") {
  const auto result = parse_extras(R"({"text": "a\"b\\c\nd é"})");
  REQUIRE(result.ok);
  CHECK(get_string(result.properties, "text") == "a\"b\\c\nd \xC3\xA9");
}

TEST_CASE("empty input is an empty object") {
  CHECK(parse_extras("").ok);
  CHECK(parse_extras("   ").properties.empty());
  CHECK(parse_extras("{}").ok);
}

TEST_CASE("malformed json reports an error") {
  for (const char* bad : {"{", R"({"a" 1})", R"({"a": tru})", R"({"a": 1,})", R"({"a": 1} x)", "[1,2]"}) {
    CAPTURE(bad);
    const auto result = parse_extras(bad);
    CHECK_FALSE(result.ok);
    CHECK_FALSE(result.error.empty());
    CHECK(result.properties.empty());
  }
}

TEST_CASE("every one-letter escape is decoded, and an unknown one is an error") {
  const auto result = parse_extras(R"({"text": "\/ \b \f \r \t"})");
  REQUIRE(result.ok);
  CHECK(get_string(result.properties, "text") == "/ \b \f \r \t");
  CHECK_FALSE(parse_extras(R"({"text": "\q"})").ok);
  CHECK_FALSE(parse_extras(R"({"text": "\u12"})").ok);
}

TEST_CASE("skipped values may hold anything, empty or nested") {
  const auto result =
      parse_extras(R"({"a": [], "b": {}, "c": [true, false, null, "x", -1.5, {"k": [1, {"deep": "]}"}]}], "ok": 2})");
  REQUIRE(result.ok);
  CHECK(result.skipped_keys == std::vector<std::string>{"a", "b", "c"});
  CHECK(get_number(result.properties, "ok") == doctest::Approx(2.0));
}

TEST_CASE("only JSON numbers are numbers: no infinity, no not-a-number, no hexadecimal, nothing endless") {
  for (const char* text : {R"({"a": 1e999})", R"({"a": -1e999})", R"({"a": 0x10})", R"({"a": -inf})", R"({"a": -nan})",
                           R"({"a": [inf]})", R"({"a": [nan]})", R"({"a": 1e})", R"({"a": -})"}) {
    CAPTURE(text);
    CHECK_FALSE(parse_extras(text).ok);
  }
  CHECK_FALSE(parse_extras(R"({"a": )" + std::string(200, '9') + "}").ok);
  const ExtrasParseResult fine = parse_extras(R"({"a": -12.5e-1, "b": 3, "c": [0.5, 1E2]})");
  REQUIRE(fine.ok);
  CHECK(get_number(fine.properties, "a") == doctest::Approx(-1.25));
  CHECK(get_number(fine.properties, "b") == doctest::Approx(3.0));
}

TEST_CASE("a value nested deeper than the limit is an error, not a stack that runs out") {
  constexpr std::size_t kVeryDeep = 200000;
  constexpr std::size_t kShallow = 20;
  const auto nested = [](std::size_t depth) {
    return R"({"a": )" + std::string(depth, '[') + std::string(depth, ']') + "}";
  };
  const ExtrasParseResult deep = parse_extras(nested(kVeryDeep));
  CHECK_FALSE(deep.ok);
  CHECK(deep.error.find("nested") != std::string::npos);
  const ExtrasParseResult shallow = parse_extras(nested(kShallow));
  CHECK(shallow.ok);
  CHECK(shallow.skipped_keys == std::vector<std::string>{"a"});
  // Nesting is counted as it closes: many values side by side are not deep.
  CHECK(parse_extras(R"({"a": [[1], [2], [[3]], {"b": [4]}], "c": [[5]]})").ok);
}

TEST_CASE("many numbers in a long text are read in the time it takes to read the text once") {
  constexpr std::size_t kNumbers = 1500000;
  constexpr double kGenerousSeconds = 5.0;  // a copy of the rest of the text per number takes minutes
  std::string text = R"({"points": [)";
  for (std::size_t i = 0; i < kNumbers; ++i) text += "1.5,";
  text += "2]}";
  const auto started = std::chrono::steady_clock::now();
  const ExtrasParseResult result = parse_extras(text);
  const std::chrono::duration<double> took = std::chrono::steady_clock::now() - started;
  CHECK(result.ok);
  CHECK(took.count() < kGenerousSeconds);
}
