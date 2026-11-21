// The GameObject menu's objects, by kind, as the editor and the tools make them.

#include <doctest/doctest.h>

#include "editor/new_object_kinds.hpp"
#include "scene/props.hpp"

using namespace redgem;
using new_objects::Kind;

namespace {
SceneDocument blank() { return SceneDocument::create_empty("new_object_kinds_test.glb"); }
}  // namespace

TEST_CASE("every kind has a word, and only those words are kinds") {
  for (const char* word : {"empty", "cube", "sphere", "plane", "cylinder", "capsule", "sun", "point_light",
                           "spot_light", "camera", "terrain", "environment"}) {
    CAPTURE(word);
    REQUIRE(new_objects::kind_named(word).has_value());
    CHECK(new_objects::kind_words().find(word) != std::string::npos);
  }
  CHECK_FALSE(new_objects::kind_named("Cube").has_value());
  CHECK_FALSE(new_objects::kind_named("light").has_value());
}

TEST_CASE("objects of each kind are what the menu makes") {
  SceneDocument document = blank();
  const int cube = new_objects::add_object(document, Kind::Cube, "Box");
  CHECK(document.node(cube).mesh != kNoIndex);
  const int sun = new_objects::add_object(document, Kind::Sun, new_objects::default_name(Kind::Sun));
  REQUIRE(document.node(sun).light.has_value());
  CHECK(document.node(sun).light->type == LightType::Directional);
  CHECK(document.node(sun).local.rotation.w != doctest::Approx(1));  // aimed down at an angle
  const int lamp = new_objects::add_object(document, Kind::PointLight, "Lamp", cube);
  CHECK(document.node(lamp).parent == cube);
  CHECK(document.node(new_objects::add_object(document, Kind::Camera, "Eye")).camera.has_value());
  CHECK(document.terrain_grid(new_objects::add_object(document, Kind::Terrain, "Ground")).has_value());
  CHECK(new_objects::environment_of(document) == kNoIndex);
  const int environment = new_objects::add_object(document, Kind::Environment, "Environment");
  CHECK(new_objects::environment_of(document) == environment);
  CHECK(document.node(environment).props.count(props::environment::kSkyTop) == 1);
}

TEST_CASE("a new object takes a numbered name when its own is taken") {
  SceneDocument document = blank();
  new_objects::add_object(document, Kind::Cube, "Box");
  const int second = new_objects::add_object(document, Kind::Sphere, "Box");
  CHECK(document.node(second).name != "Box");
  CHECK(document.node(second).name.rfind("Box", 0) == 0);
}
