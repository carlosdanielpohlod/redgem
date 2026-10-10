#include <doctest/doctest.h>

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
