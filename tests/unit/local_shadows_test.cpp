// Shadows of point and spot lights: which lights cast them in a frame, within the budget and the
// atlas, how each tile looks out, and how a spot dims within its cone.
#include <doctest/doctest.h>

#include <cmath>
#include <vector>

#include "render/environment.hpp"
#include "render/local_shadows.hpp"
#include "render/scene_lights.hpp"
#include "scene/props.hpp"
#include "test_support.hpp"

using namespace redgem;
using namespace redgem::render_math;
using redgem::test::kTolerance;

namespace {

constexpr float kRange = 10.0f;     // meters
constexpr float kInnerCone = 0.3f;  // radians
constexpr float kOuterCone = 0.6f;
constexpr float kTooWide = 2.0f;           // radians: past a right angle
constexpr int kAllOfThem = 99;             // a budget past the most there can be
constexpr std::size_t kPointsThatFit = 2;  // six tiles each, of sixteen

PointLight light(bool spot, bool cast_shadow) {
  PointLight out{Vec3::zero(), Vec3::one(), kRange};
  out.spot = spot;
  out.axis = Vec3{0.0f, -1.0f, 0.0f};
  out.outer_cone = kOuterCone;
  out.cast_shadow = cast_shadow;
  return out;
}

PointLight casting_point() { return light(false, true); }
PointLight casting_spot() { return light(true, true); }

// How much of a spot's light goes `off_axis` radians from its axis (glTF's cone falloff).
float cone_light(const SpotCone& cone, float off_axis) {
  const float share = std::fmin(std::fmax(std::cos(off_axis) * cone.scale + cone.offset, 0.0f), 1.0f);
  return share * share;
}

}  // namespace

TEST_CASE("a spot shines fully inside its inner cone, dims smoothly, and nothing reaches past its outer cone") {
  const SpotCone cone = spot_cone(kInnerCone, kOuterCone);
  CHECK(cone_light(cone, 0.0f) == doctest::Approx(1.0f));
  CHECK(cone_light(cone, kInnerCone) == doctest::Approx(1.0f));
  CHECK(cone_light(cone, kOuterCone) == doctest::Approx(0.0f).epsilon(kTolerance));
  const float halfway = cone_light(cone, (kInnerCone + kOuterCone) * kHalf);
  CHECK(halfway > 0.0f);
  CHECK(halfway < 1.0f);
  // A point light (the default cone) shines the same every way.
  CHECK(cone_light(SpotCone{}, kTooWide) == doctest::Approx(1.0f));
  // A cone with no dimming edge, or one wider than a right angle, still gives finite numbers.
  CHECK(std::isfinite(spot_cone(kOuterCone, kOuterCone).scale));
  CHECK(std::isfinite(spot_cone(kTooWide, kTooWide).offset));
}

TEST_CASE("only lights whose cast_shadow is on cast shadows, in the order they matter, within the budget") {
  const std::vector<PointLight> lights = {light(true, false), casting_spot(), light(false, false), casting_spot(),
                                          casting_spot()};
  const std::vector<LocalShadow> two = pick_local_shadows(lights, 2);
  REQUIRE(two.size() == 2);
  CHECK(two[0].light == 1);
  CHECK(two[1].light == 3);
  CHECK(two[0].first_tile == 0);
  CHECK(two[1].first_tile == kSpotTiles);
  CHECK(pick_local_shadows(lights, 0).empty());
  CHECK(pick_local_shadows(lights, -1).empty());
  CHECK(pick_local_shadows(lights, kAllOfThem).size() == 3);
  CHECK(pick_local_shadows({light(true, false), light(false, false)}, kAllOfThem).empty());
}

TEST_CASE("the budget never passes the most the shader holds") {
  const std::vector<PointLight> spots(kMaxLocalShadows + 2, casting_spot());
  CHECK(pick_local_shadows(spots, kAllOfThem).size() == static_cast<std::size_t>(kMaxLocalShadows));
}

TEST_CASE("a point light takes six tiles; one that no longer fits in the atlas leaves room for a spot") {
  const std::vector<PointLight> lights = {casting_point(), casting_point(), casting_point(), casting_spot()};
  const std::vector<LocalShadow> picked = pick_local_shadows(lights, kAllOfThem);
  REQUIRE(picked.size() == kPointsThatFit + 1);
  CHECK(picked[0].tiles == kCubeFaces);
  CHECK(picked[1].first_tile == kCubeFaces);
  CHECK(picked[2].light == 3);  // the third point did not fit
  CHECK(picked[2].first_tile == kCubeFaces * 2);
  CHECK(picked[2].first_tile + picked[2].tiles <= kLocalShadowTiles);
}

TEST_CASE("the lights that cast shadows come first, and each shadow follows its light") {
  std::vector<PointLight> lights = {light(false, false), casting_spot(), light(true, false), casting_point()};
  lights[1].range = 1.0f;
  lights[3].range = 2.0f;
  std::vector<LocalShadow> shadows = pick_local_shadows(lights, kAllOfThem);
  const std::vector<PointLight> ordered = shadowed_first(lights, shadows);
  REQUIRE(ordered.size() == lights.size());
  CHECK(shadows[0].light == 0);
  CHECK(shadows[1].light == 1);
  CHECK(ordered[0].range == doctest::Approx(1.0f));
  CHECK(ordered[1].range == doctest::Approx(2.0f));
  CHECK_FALSE(ordered[2].spot);  // then the others, in their order
  CHECK(ordered[3].spot);
}

TEST_CASE("a point light's six tiles look along each axis, and no tile's up is its forward") {
  const Vec3 expected[] = {{1.0f, 0.0f, 0.0f},  {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
                           {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f},  {0.0f, 0.0f, -1.0f}};
  for (int face = 0; face < kCubeFaces; ++face) {
    const ShadowFace view = cube_face(face);
    CHECK(approx_equal(view.forward, expected[face], kTolerance));
    CHECK(std::fabs(dot(view.forward, view.up)) < kTolerance);
  }
  const PointLight down = casting_spot();
  CHECK(approx_equal(spot_face(down).up, Vec3{0.0f, 0.0f, 1.0f}, kTolerance));
}

TEST_CASE("a spot's shadow view is as wide as its cone, and never a right angle") {
  PointLight spot = casting_spot();
  CHECK(spot_spread(spot) > std::tan(kOuterCone));
  spot.outer_cone = kTooWide;
  CHECK(spot_spread(spot) == doctest::Approx(std::tan(kWidestSpotShadow)));
}

TEST_CASE("the atlas's tiles lie side by side, row after row, all inside it") {
  const float size = 1.0f / static_cast<float>(kLocalTilesAcross);
  CHECK(shadow_tile(0).x == 0.0f);
  CHECK(shadow_tile(1).x == doctest::Approx(size));
  CHECK(shadow_tile(kLocalTilesAcross).y == doctest::Approx(size));
  const ShadowTile last = shadow_tile(kLocalShadowTiles - 1);
  CHECK(last.x + last.size == doctest::Approx(1.0f));
  CHECK(last.y + last.size == doctest::Approx(1.0f));
}

TEST_CASE("a level asks for how many lights cast shadows; quality and the card hold it down") {
  PropertyMap props;
  CHECK(environment_from_props(props).local_shadows == config::kMostLocalShadows);
  props[props::environment::kLocalShadows] = 1.0;
  CHECK(environment_from_props(props).local_shadows == 1);
  props[props::environment::kLocalShadows] = static_cast<double>(kAllOfThem);
  CHECK(environment_from_props(props).local_shadows == config::kMostLocalShadows);
  props[props::environment::kLocalShadows] = -1.0;
  CHECK(environment_from_props(props).local_shadows == 0);

  const Environment asked = default_environment();
  CHECK(within_quality(asked, preset_caps(QualityPreset::Ultra), GpuAbilities{}).local_shadows ==
        config::kMostLocalShadows);
  CHECK(within_quality(asked, preset_caps(QualityPreset::Low), GpuAbilities{}).local_shadows == 0);
  GpuAbilities without_atlas;
  without_atlas.local_shadows = false;
  CHECK(within_quality(asked, preset_caps(QualityPreset::Ultra), without_atlas).local_shadows == 0);
  Environment fewer = asked;
  fewer.local_shadows = 1;
  CHECK(within_quality(fewer, preset_caps(QualityPreset::Ultra), GpuAbilities{}).local_shadows == 1);
}
