#include <doctest/doctest.h>

#include "render/environment.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::kTolerance;

TEST_CASE("hex colors are read as sRGB and become linear") {
  const auto white = parse_hex_color("#ffffff");
  REQUIRE(white.has_value());
  CHECK(approx_equal(*white, Vec3::one(), kTolerance));
  const auto mixed = parse_hex_color("#FF8000");
  REQUIRE(mixed.has_value());
  CHECK(mixed->x == doctest::Approx(1.0f));
  CHECK(mixed->y == doctest::Approx(0.2195f).epsilon(0.01));  // (128/255)^2.2
  CHECK(mixed->z == doctest::Approx(0.0f));
  CHECK_FALSE(parse_hex_color("ff8000").has_value());
  CHECK_FALSE(parse_hex_color("#ff80").has_value());
  CHECK_FALSE(parse_hex_color("#gg8000").has_value());
}

TEST_CASE("an environment object overrides only what it sets") {
  constexpr double kFog = 0.02;
  constexpr double kBloom = 0.8;
  const Environment defaults = default_environment();
  PropertyMap props;
  props["sky_horizon"] = std::string("#ff8000");
  props["fog_density"] = kFog;
  props["bloom"] = kBloom;
  props["exposure"] = -3.0;            // negative strengths make no sense: clamped to 0
  props["sky_top"] = std::string("oops");  // malformed: keeps the default
  const Environment env = environment_from_props(props);
  CHECK(env.sky_horizon.x == doctest::Approx(1.0f));
  CHECK(approx_equal(env.fog_color, env.sky_horizon, kTolerance));  // fog follows the horizon
  CHECK(env.fog_density == doctest::Approx(kFog));
  CHECK(env.bloom == doctest::Approx(kBloom));
  CHECK(env.exposure == doctest::Approx(0.0f));
  CHECK(approx_equal(env.sky_top, defaults.sky_top, kTolerance));
  CHECK(env.ambient == doctest::Approx(defaults.ambient));

  props["fog_color"] = std::string("#000000");
  CHECK(approx_equal(environment_from_props(props).fog_color, Vec3::zero(), kTolerance));
}

TEST_CASE("a scene uses its environment object, or a clear day without one") {
  SceneDesc desc;
  NodeDesc plain;
  plain.name = "Thing";
  desc.nodes.push_back(plain);
  CHECK(scene_environment(Scene::from_desc(desc)).fog_density == doctest::Approx(default_environment().fog_density));
  NodeDesc sky;
  sky.name = "Environment";
  sky.props["environment"] = true;
  sky.props["wind"] = 2.0;
  desc.nodes.push_back(sky);
  CHECK(scene_environment(Scene::from_desc(desc)).wind == doctest::Approx(2.0f));
}

TEST_CASE("the lights nearest to the viewer are kept, counting their range") {
  constexpr float kSmall = 5.0f;
  constexpr float kHuge = 80.0f;
  const std::vector<PointLight> lights = {
      {{100.0f, 0.0f, 0.0f}, Vec3::one(), kSmall},  // far
      {{3.0f, 0.0f, 0.0f}, Vec3::one(), kSmall},    // near
      {{60.0f, 0.0f, 0.0f}, Vec3::one(), kHuge},    // far but it reaches the viewer
      {{20.0f, 0.0f, 0.0f}, Vec3::one(), kSmall},
  };
  const auto kept = nearest_lights(lights, Vec3::zero(), 2);
  REQUIRE(kept.size() == 2);
  CHECK(kept[0].position.x == doctest::Approx(60.0f));
  CHECK(kept[1].position.x == doctest::Approx(3.0f));
  CHECK(nearest_lights(lights, Vec3::zero(), 10).size() == lights.size());
  CHECK(nearest_lights({}, Vec3::zero(), 4).empty());
}

TEST_CASE("lights about to be dropped are dimmed, so they do not pop when the viewer moves") {
  constexpr float kRange = 5.0f;
  constexpr float kFade = 10.0f;
  const std::vector<PointLight> lights = {
      {{10.0f, 0.0f, 0.0f}, Vec3::one(), kRange},   // well inside
      {{28.0f, 0.0f, 0.0f}, Vec3::one(), kRange},   // 2 m before the cut: mostly faded
      {{30.0f, 0.0f, 0.0f}, Vec3::one(), kRange},   // left out: marks the cut
  };
  const auto kept = nearest_lights(lights, Vec3::zero(), 2, kFade);
  REQUIRE(kept.size() == 2);
  CHECK(kept[0].color.x == doctest::Approx(1.0f));
  CHECK(kept[1].color.x == doctest::Approx(0.2f));
  // Nothing is left out: nothing fades.
  CHECK(nearest_lights(lights, Vec3::zero(), 3, kFade)[2].color.x == doctest::Approx(1.0f));
}
