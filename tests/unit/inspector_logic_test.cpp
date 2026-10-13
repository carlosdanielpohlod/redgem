#include <doctest/doctest.h>

#include <optional>
#include <string>

#include "editor/inspector_logic.hpp"
#include "scene/props.hpp"

using namespace redgem;
using namespace redgem::inspector_logic;

namespace {

constexpr const char* kSpeed = "speed";
constexpr double kFast = 4.0;
constexpr int kSomeImage = 2;

}  // namespace

TEST_CASE("a flag is stored only away from its default, and a text only when it says something") {
  CHECK(flag_or_nothing(true, false) == std::optional<PropertyValue>(false));
  CHECK(flag_or_nothing(true, true) == std::optional<PropertyValue>(true));
  CHECK_FALSE(flag_or_nothing(false, true).has_value());
  CHECK(text_or_nothing("enemy") == std::optional<PropertyValue>(std::string("enemy")));
  CHECK_FALSE(text_or_nothing("").has_value());
}

TEST_CASE("a new property starts as zero, an empty text or off") {
  CHECK(initial_value(PropertyKind::Number) == PropertyValue(0.0));
  CHECK(initial_value(PropertyKind::Text) == PropertyValue(std::string()));
  CHECK(initial_value(PropertyKind::Toggle) == PropertyValue(false));
}

TEST_CASE("an object has a Physics card for a body, a trigger or a collider alone") {
  CHECK_FALSE(has_physics({}));
  CHECK_FALSE(has_physics({{kSpeed, kFast}}));
  CHECK(has_physics({{props::kBody, std::string(props::kBodyTypes[0])}}));
  CHECK(has_physics({{props::kTrigger, true}}));
  CHECK(has_physics({{props::kCollider, std::string(props::kColliders[0])}}));
}

TEST_CASE("only the bodies gravity moves have a mass to set") {
  BodyConfig body;
  for (const BodyType type : {BodyType::Dynamic, BodyType::Vehicle}) {
    body.type = type;
    CHECK(has_mass(body));
  }
  for (const BodyType type : {BodyType::None, BodyType::Static, BodyType::Kinematic, BodyType::Character}) {
    body.type = type;
    CHECK_FALSE(has_mass(body));
  }
}

TEST_CASE("properties with a widget of their own stay out of the list, and their names cannot be taken") {
  for (const char* reserved : {props::kScript, props::kBody, props::kCollider, props::kTrigger, props::kMass,
                               props::kTag, props::kWheel, props::kVisible, props::kPrefab, props::kTerrain}) {
    CAPTURE(reserved);
    CHECK_FALSE(is_custom(reserved));
    CHECK_FALSE(can_add_property({}, reserved));
  }
  CHECK(is_custom(kSpeed));
  CHECK(can_add_property({}, kSpeed));
  CHECK_FALSE(can_add_property({{kSpeed, kFast}}, kSpeed));
  CHECK_FALSE(can_add_property({}, ""));
}

TEST_CASE("a material is textured by a color picture or a normal map") {
  MaterialDesc material;
  CHECK_FALSE(textured(material));
  material.normal_image = kSomeImage;
  CHECK(textured(material));
  material.normal_image = kNoIndex;
  material.base_color_image = kSomeImage;
  CHECK(textured(material));
}

TEST_CASE("the wheel card is for wheels and for the children of a vehicle") {
  const std::optional<std::string> vehicle = std::string(props::kBodyVehicle);
  const std::optional<std::string> crate = std::string(props::kBodyTypes[0]);
  CHECK(wheel_card_shown(vehicle, false));
  CHECK(wheel_card_shown(crate, true));
  CHECK(wheel_card_shown(std::nullopt, true));
  CHECK_FALSE(wheel_card_shown(crate, false));
  CHECK_FALSE(wheel_card_shown(std::nullopt, false));
}
