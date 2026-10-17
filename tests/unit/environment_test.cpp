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
  props["exposure"] = -3.0;                // negative strengths make no sense: clamped to 0
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

TEST_CASE("the picture's grade is neutral unless the environment sets one") {
  const Environment plain = environment_from_props({});
  CHECK(plain.contrast == doctest::Approx(1.0f));
  CHECK(plain.saturation == doctest::Approx(1.0f));
  CHECK(plain.grain == doctest::Approx(0.0f));
  CHECK(plain.aberration == doctest::Approx(0.0f));
  const Environment graded =
      environment_from_props({{"contrast", 1.2}, {"saturation", 0.5}, {"grain", 0.4}, {"aberration", 0.7}});
  CHECK(graded.contrast == doctest::Approx(1.2f));
  CHECK(graded.saturation == doctest::Approx(0.5f));
  CHECK(graded.grain == doctest::Approx(0.4f));
  CHECK(graded.aberration == doctest::Approx(0.7f));
}

namespace {

constexpr double kTooStrong = 3.0;  // more than a 0..1 strength can be
constexpr float kFull = 1.0f;

}  // namespace

TEST_CASE("strengths that go from none to full are held at full") {
  const Environment env = environment_from_props(
      {{"shadows", kTooStrong}, {"vignette", kTooStrong}, {"ambient_occlusion", kTooStrong}, {"bloom", kTooStrong}});
  CHECK(env.shadows == doctest::Approx(kFull));
  CHECK(env.vignette == doctest::Approx(kFull));
  CHECK(env.ambient_occlusion == doctest::Approx(kFull));
  CHECK(env.bloom == doctest::Approx(kTooStrong));  // a plain amount: as much as asked
}

TEST_CASE("glowing particles have no limit unless the environment sets one") {
  constexpr double kLimit = 3.0;
  CHECK(default_environment().glow_limit == doctest::Approx(0.0f));
  PropertyMap props;
  props["glow_limit"] = kLimit;
  CHECK(environment_from_props(props).glow_limit == doctest::Approx(kLimit));
}
