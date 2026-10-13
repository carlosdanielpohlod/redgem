#include <doctest/doctest.h>

#include <vector>

#include "render/render_config.hpp"
#include "render/scene_lights.hpp"
#include "scene/props.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::kTolerance;

namespace {

constexpr float kSunWatts = 5.0f;      // W/m^2, as exported
constexpr float kLampWatts = 1000.0f;  // Watts
constexpr float kLampRange = 12.0f;    // meters
constexpr Vec3 kLampPlace{4.0f, 3.0f, -2.0f};
constexpr redgem::Color kWarm{1.0f, 0.5f, 0.25f, 1.0f};
constexpr float kQuarterTurnDown = -90.0f;  // degrees around x: looking straight down
constexpr std::size_t kOneLamp = 1;

NodeDesc light_node(LightType type, float intensity) {
  NodeDesc node;
  LightDesc light;
  light.type = type;
  light.color = kWarm;
  light.intensity = intensity;
  node.light = light;
  return node;
}

Vec3 default_sun_direction() {
  return normalize(Vec3{config::kDefaultSunDirectionX, config::kDefaultSunDirectionY, config::kDefaultSunDirectionZ});
}

}  // namespace

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
      {{10.0f, 0.0f, 0.0f}, Vec3::one(), kRange},  // well inside
      {{28.0f, 0.0f, 0.0f}, Vec3::one(), kRange},  // 2 m before the cut: mostly faded
      {{30.0f, 0.0f, 0.0f}, Vec3::one(), kRange},  // left out: marks the cut
  };
  const auto kept = nearest_lights(lights, Vec3::zero(), 2, kFade);
  REQUIRE(kept.size() == 2);
  CHECK(kept[0].color.x == doctest::Approx(1.0f));
  CHECK(kept[1].color.x == doctest::Approx(0.2f));
  // Nothing is left out: nothing fades.
  CHECK(nearest_lights(lights, Vec3::zero(), 3, kFade)[2].color.x == doctest::Approx(1.0f));
}

TEST_CASE("a scene without lights is lit by a default sun and nothing else") {
  const SceneLights lights = scene_lights(Scene::from_desc(SceneDesc{}));
  CHECK(approx_equal(lights.sun_direction, default_sun_direction(), kTolerance));
  CHECK(approx_equal(lights.sun_color, Vec3::one() * config::kDefaultSunIntensity, kTolerance));
  CHECK(lights.points.empty());
}

TEST_CASE("the first directional light is the sun, shining the way it points") {
  SceneDesc desc;
  NodeDesc sun = light_node(LightType::Directional, kSunWatts);
  sun.local.rotation = quat_from_euler_degrees({kQuarterTurnDown, 0.0f, 0.0f});
  NodeDesc second_sun = light_node(LightType::Directional, kLampWatts);
  desc.nodes = {sun, second_sun};
  const SceneLights lights = scene_lights(Scene::from_desc(desc));
  CHECK(approx_equal(lights.sun_direction, -Vec3::up(), kTolerance));
  const float strength = kSunWatts * config::kSunStrengthScale;
  CHECK(approx_equal(lights.sun_color, Vec3{kWarm.r, kWarm.g, kWarm.b} * strength, kTolerance));
  CHECK(lights.points.empty());  // a second sun is ignored, not turned into a lamp
}

TEST_CASE("every other light shines from where it is, as far as its range or the default one") {
  SceneDesc desc;
  NodeDesc lamp = light_node(LightType::Point, kLampWatts);
  lamp.local.position = kLampPlace;
  lamp.light->range = kLampRange;
  NodeDesc unbounded = light_node(LightType::Point, kLampWatts);
  desc.nodes = {lamp, unbounded};
  const SceneLights lights = scene_lights(Scene::from_desc(desc));
  REQUIRE(lights.points.size() == desc.nodes.size());
  CHECK(approx_equal(lights.points[0].position, kLampPlace, kTolerance));
  CHECK(lights.points[0].range == doctest::Approx(kLampRange));
  const float strength = kLampWatts * config::kPointWattScale;
  CHECK(approx_equal(lights.points[0].color, Vec3{kWarm.r, kWarm.g, kWarm.b} * strength, kTolerance));
  CHECK(lights.points[1].range == doctest::Approx(config::kDefaultPointLightRange));
}

TEST_CASE("the lights of prefabs do not shine until they are spawned") {
  SceneDesc desc;
  NodeDesc lamp = light_node(LightType::Point, kLampWatts);
  NodeDesc prefab_lamp = light_node(LightType::Point, kLampWatts);
  prefab_lamp.props[props::kPrefab] = true;
  desc.nodes = {lamp, prefab_lamp};
  CHECK(scene_lights(Scene::from_desc(desc)).points.size() == kOneLamp);
}
