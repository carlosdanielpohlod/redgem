#include <doctest/doctest.h>

#include "scene/property.hpp"

using namespace redgem;

TEST_CASE("typed property getters") {
  const PropertyMap props{{"name", std::string("hero")}, {"speed", 2.5}, {"on", true}, {"flag", 1.0}};
  CHECK(get_string(props, "name") == "hero");
  CHECK(get_number(props, "speed") == doctest::Approx(2.5));
  CHECK(get_bool(props, "on") == true);
  CHECK(get_bool(props, "flag") == true);  // Blender sometimes stores toggles as 0/1
  CHECK_FALSE(get_string(props, "speed").has_value());
  CHECK_FALSE(get_number(props, "missing").has_value());
  CHECK_FALSE(get_bool(props, "name").has_value());
}
