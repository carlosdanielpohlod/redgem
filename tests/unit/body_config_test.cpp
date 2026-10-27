#include <doctest/doctest.h>

#include <string>
#include <utility>

#include "physics/physics_config.hpp"
#include "physics/body_config.hpp"
#include "scene/props.hpp"

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
  const BodyConfig forced =
      body_config_from_props({{"body", std::string("character")}, {"collider", std::string("box")}});
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
  const BodyConfig config =
      body_config_from_props({{"body", std::string("dynamic")}, {"collider", std::string("mesh")}});
  CHECK(config.collider == ColliderShape::Box);
  CHECK(config.warnings.size() == 1);
  CHECK(body_config_from_props({{"collider", std::string("mesh")}}).collider == ColliderShape::Mesh);
}

TEST_CASE("unknown values produce warnings") {
  const BodyConfig config =
      body_config_from_props({{"body", std::string("floaty")}, {"collider", std::string("blob")}});
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

TEST_CASE("body types have the names scenes use, but none cannot be asked for") {
  CHECK(std::string(body_type_name(BodyType::Kinematic)) == "kinematic");
  CHECK(std::string(body_type_name(BodyType::None)) == "none");
  for (const BodyType type : {BodyType::Static, BodyType::Dynamic, BodyType::Kinematic, BodyType::Vehicle}) {
    CAPTURE(body_type_name(type));
    CHECK(body_config_from_props({{"body", std::string(body_type_name(type))}}).type == type);
  }
  const BodyConfig none = body_config_from_props({{"body", std::string("none")}});
  CHECK(none.type == BodyType::None);
  CHECK(none.warnings.size() == 1);
}

TEST_CASE("every body type and collider a list offers is one the physics knows") {
  for (const char* name : props::kBodyTypes) {
    CAPTURE(name);
    const BodyConfig config = body_config_from_props({{"body", std::string(name)}});
    CHECK(std::string(body_type_name(config.type)) == name);
    CHECK(config.warnings.empty());
  }
  for (const char* name : props::kColliders) {
    CAPTURE(name);
    CHECK(body_config_from_props({{"collider", std::string(name)}}).warnings.empty());
  }
  CHECK(body_config_from_props({{"body", std::string(props::kBodyStatic)}}).type == BodyType::Static);
}

TEST_CASE("an object without a mesh collides as a small cube around it") {
  const Bounds bounds = default_collider_bounds();
  CHECK(bounds.half_extents().x == doctest::Approx(config::kDefaultColliderHalfExtent));
  CHECK(bounds.half_extents().y == doctest::Approx(config::kDefaultColliderHalfExtent));
  CHECK(bounds.half_extents().z == doctest::Approx(config::kDefaultColliderHalfExtent));
}

TEST_CASE("each collider name gives its shape") {
  const std::pair<const char*, ColliderShape> colliders[] = {{props::kColliderBox, ColliderShape::Box},
                                                             {props::kColliderSphere, ColliderShape::Sphere},
                                                             {props::kColliderCapsule, ColliderShape::Capsule},
                                                             {props::kColliderMesh, ColliderShape::Mesh}};
  for (const auto& collider : colliders) {
    // (Not a structured binding: CAPTURE keeps it in a lambda, which C++17 does not allow.)
    const char* name = collider.first;
    CAPTURE(name);
    const BodyConfig config = body_config_from_props({{"collider", std::string(name)}});
    CHECK(config.collider == collider.second);
    CHECK(config.type == BodyType::Static);
    CHECK(config.warnings.empty());
  }
}

TEST_CASE("vehicles always use a box, and weigh more than other bodies by default") {
  const BodyConfig config =
      body_config_from_props({{"body", std::string("vehicle")}, {"collider", std::string("sphere")}});
  CHECK(config.collider == ColliderShape::Box);
  CHECK(config.warnings.size() == 1);
  CHECK(config.mass == doctest::Approx(config::kVehicleDefaultMass));
}

TEST_CASE("forces move dynamic bodies and vehicles, and nothing else") {
  CHECK(is_dynamic(BodyType::Dynamic));
  CHECK(is_dynamic(BodyType::Vehicle));
  CHECK_FALSE(is_dynamic(BodyType::None));
  CHECK_FALSE(is_dynamic(BodyType::Static));
  CHECK_FALSE(is_dynamic(BodyType::Kinematic));
  CHECK_FALSE(is_dynamic(BodyType::Character));
}

TEST_CASE("a trigger that is also static, or has a collider of its own, says nothing and stays a volume") {
  const BodyConfig still =
      body_config_from_props({{"trigger", true}, {"body", std::string("static")}, {"collider", std::string("sphere")}});
  CHECK(still.trigger);
  CHECK(still.type == BodyType::None);
  CHECK(still.collider == ColliderShape::Sphere);
  CHECK(still.warnings.empty());
}

TEST_CASE("a mesh collider stays on what does not move and becomes a box on what does") {
  const BodyConfig ground =
      body_config_from_props({{"body", std::string("static")}, {"collider", std::string("mesh")}});
  CHECK(ground.collider == ColliderShape::Mesh);
  CHECK(ground.warnings.empty());
  const BodyConfig lift =
      body_config_from_props({{"body", std::string("kinematic")}, {"collider", std::string("mesh")}});
  CHECK(lift.collider == ColliderShape::Box);
  CHECK(lift.warnings.size() == 1);
}
