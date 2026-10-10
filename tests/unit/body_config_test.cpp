#include <doctest/doctest.h>

#include "core/config.hpp"
#include "physics/body_config.hpp"

using namespace redgem;

TEST_CASE("objects without physics props have no body") {
  const BodyConfig config = body_config_from_props({{"script", std::string("a.rb")}});
  CHECK_FALSE(config.wants_physics());
}

TEST_CASE("collider alone means a static body") {
  const BodyConfig config = body_config_from_props({{"collider", std::string("sphere")}});
  CHECK(config.type == BodyType::Static);
  CHECK(config.collider == ColliderShape::Sphere);
}

TEST_CASE("dynamic body reads its mass") {
  const BodyConfig config = body_config_from_props({{"body", std::string("dynamic")}, {"mass", 4.0}});
  CHECK(config.type == BodyType::Dynamic);
  CHECK(config.collider == ColliderShape::Box);
  CHECK(config.mass == doctest::Approx(4.0f));
}

TEST_CASE("characters always use capsules") {
  CHECK(body_config_from_props({{"body", std::string("character")}}).collider == ColliderShape::Capsule);
  const BodyConfig forced = body_config_from_props({{"body", std::string("character")}, {"collider", std::string("box")}});
  CHECK(forced.collider == ColliderShape::Capsule);
  CHECK(forced.warnings.size() == 1);
}

TEST_CASE("triggers are volumes, not bodies") {
  const BodyConfig config = body_config_from_props({{"trigger", true}, {"body", std::string("dynamic")}});
  CHECK(config.trigger);
  CHECK(config.type == BodyType::None);
  CHECK(config.wants_physics());
  CHECK(config.warnings.size() == 1);
}

TEST_CASE("mesh colliders only on static bodies") {
  const BodyConfig config = body_config_from_props({{"body", std::string("dynamic")}, {"collider", std::string("mesh")}});
  CHECK(config.collider == ColliderShape::Box);
  CHECK(config.warnings.size() == 1);
  CHECK(body_config_from_props({{"collider", std::string("mesh")}}).collider == ColliderShape::Mesh);
}

TEST_CASE("unknown values produce warnings") {
  const BodyConfig config = body_config_from_props({{"body", std::string("floaty")}, {"collider", std::string("blob")}});
  CHECK(config.type == BodyType::None);
  CHECK(config.warnings.size() == 2);
}

TEST_CASE("non-positive mass falls back to the default with a warning") {
  for (const double mass : {0.0, -3.0}) {
    CAPTURE(mass);
    const BodyConfig config = body_config_from_props({{"body", std::string("dynamic")}, {"mass", mass}});
    CHECK(config.mass == doctest::Approx(config::kDefaultBodyMass));
    CHECK(config.warnings.size() == 1);
  }
}
