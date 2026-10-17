// The light of a sky picture: how it is laid around the world, what matte surfaces receive of it
// (spherical harmonics) and what glossy ones reflect (the split sum's cube map and table).
#include <doctest/doctest.h>

#include <cmath>

#include "render/half_float.hpp"
#include "render/sky_directions.hpp"
#include "render/sky_harmonics.hpp"
#include "render/sky_light.hpp"
#include "render/sky_specular.hpp"
#include "test_support.hpp"

using namespace redgem;
using namespace redgem::render_math;
using redgem::test::kTolerance;

namespace {

constexpr std::uint32_t kWide = 64;  // pixels of the test skies
constexpr std::uint32_t kHigh = 32;
constexpr float kLoose = 0.02f;  // nine harmonics only approximate a sky
constexpr float kHemisphere = 0.03f;
constexpr float kQuarterTurn = kPi * kHalf;
constexpr std::uint16_t kHalfOne = 0x3C00;
constexpr std::uint16_t kHalfTwo = 0x4000;
constexpr std::uint16_t kLargestHalf = 0x7BFF;
constexpr float kBeyondHalves = 1.0e6f;
constexpr float kBrightSun = 5000.0f;

// A sky of `above` over the horizon and `below` under it.
FloatImage two_halves(Vec3 above, Vec3 below) {
  FloatImage sky = black_image(kWide, kHigh);
  for (std::uint32_t y = 0; y < kHigh; ++y) {
    for (std::uint32_t x = 0; x < kWide; ++x) add_share(sky, x, y, y < kHigh / 2 ? above : below, 1.0f);
  }
  return sky;
}

}  // namespace

TEST_CASE("a sky picture is laid as Blender lays a world's: its middle along +X, the zenith along its top") {
  CHECK(approx_equal(picture_direction({kHalf, kHalf}), Vec3::right(), kTolerance));
  CHECK(approx_equal(picture_direction({kHalf, 0.0f}), Vec3::up(), kTolerance));
  CHECK(approx_equal(picture_direction({0.75f, kHalf}), Vec3::back(), kTolerance));  // turning right: toward +Z
  for (const PictureSpot spot : {PictureSpot{0.1f, 0.3f}, PictureSpot{0.8f, 0.7f}}) {
    const PictureSpot back = picture_spot(picture_direction(spot));
    CHECK(back.u == doctest::Approx(spot.u));
    CHECK(back.v == doctest::Approx(spot.v));
  }
}

TEST_CASE("a sky turned a quarter around the vertical brings +X to +Z") {
  CHECK(approx_equal(turned_around_up(Vec3::right(), kQuarterTurn), Vec3::back(), kTolerance));
  CHECK(approx_equal(turned_around_up(Vec3::up(), kQuarterTurn), Vec3::up(), kTolerance));
}

TEST_CASE("the middle of each cube map face looks along its axis, the graphics card's way") {
  const Vec3 axes[kCubeMapFaces] = {Vec3::right(), -Vec3::right(), Vec3::up(),
                                    -Vec3::up(),   Vec3::back(),   Vec3::forward()};
  for (int face = 0; face < kCubeMapFaces; ++face) {
    CHECK(approx_equal(cube_direction(face, kHalf, kHalf), axes[face], kTolerance));
  }
  // +X's first row is its top (+Y), and its texture's s runs toward -Z.
  CHECK(cube_direction(0, kHalf, 0.0f).y > 0.0f);
  CHECK(cube_direction(0, 1.0f, kHalf).z < 0.0f);
}

TEST_CASE("under an even sky every surface receives as much light as the sky gives") {
  const SkyHarmonics even = sky_irradiance(two_halves(Vec3::one(), Vec3::one()));
  for (const Vec3 normal : {Vec3::up(), -Vec3::up(), Vec3::right(), Vec3{1.0f, 1.0f, 0.0f}}) {
    CHECK(approx_equal(irradiance_toward(even, normal), Vec3::one(), kLoose));
  }
}

TEST_CASE("a surface facing a bright sky over a dark ground receives the sky, facing down the ground") {
  const SkyHarmonics sky = sky_irradiance(two_halves(Vec3::one(), Vec3::zero()));
  CHECK(irradiance_toward(sky, Vec3::up()).x == doctest::Approx(1.0f).epsilon(kHemisphere));
  CHECK(irradiance_toward(sky, -Vec3::up()).x == doctest::Approx(0.0f).epsilon(kHemisphere));
  CHECK(irradiance_toward(sky, Vec3::right()).x == doctest::Approx(kHalf).epsilon(kHemisphere));
}

TEST_CASE("the harmonics go to the shader as three floats each, in order") {
  SkyHarmonics harmonics{};
  harmonics[1] = Vec3{1.0f, 2.0f, 3.0f};
  const HarmonicFloats floats = harmonic_floats(harmonics);
  CHECK(floats[3] == 1.0f);
  CHECK(floats[5] == 3.0f);
}

TEST_CASE("a picture's sun is held down before it lights the scene, its color kept") {
  FloatImage sky = black_image(1, 1);
  add_share(sky, 0, 0, Vec3{kBrightSun, kBrightSun * kHalf, 0.0f}, 1.0f);
  const Vec3 held = held_to_sky_light(sky).at(0, 0);
  CHECK(held.x == doctest::Approx(kBrightestSkyLight));
  CHECK(held.y == doctest::Approx(kBrightestSkyLight * kHalf));
}

TEST_CASE("the sky's cube map has every level of every face, the sharpest one the picture itself") {
  const FloatImage sky = two_halves(Vec3{2.0f, 2.0f, 2.0f}, Vec3::zero());
  const std::vector<FloatImage> pyramid = picture_pyramid(sky);
  CHECK(pyramid.back().height == 1);
  std::size_t texels = 0;
  for (int level = 0; level < kSkyCubeLevels; ++level) {
    const auto side = static_cast<std::size_t>(std::max(kSkyCubeSize >> level, 1));
    texels += side * side * kCubeMapFaces;
  }
  const std::vector<std::uint16_t> cube = sky_reflection_cube(pyramid);
  REQUIRE(cube.size() == texels * kHalfsPerTexel);
  // The middle of the +Y face's sharpest level looks straight up, at the sky's 2.0.
  const std::size_t middle = static_cast<std::size_t>(kSkyCubeSize) * kSkyCubeSize * 2 +
                             static_cast<std::size_t>(kSkyCubeSize / 2) * kSkyCubeSize + kSkyCubeSize / 2;
  CHECK(cube[middle * kHalfsPerTexel] == kHalfTwo);
}

TEST_CASE("reflections taken at random lean further from the mirror direction on rougher surfaces") {
  float first = 0.0f;
  float second = 0.0f;
  spread_pair(3, 8, first, second);
  CHECK(first == doctest::Approx(3.0f / 8.0f));
  CHECK(second == doctest::Approx(0.75f));  // 3 = 011: reversed, .110
  const float smooth = dot(sample_reflection(Vec3::up(), 0.1f, first, second).direction, Vec3::up());
  const float rough = dot(sample_reflection(Vec3::up(), 0.9f, first, second).direction, Vec3::up());
  CHECK(smooth > rough);
  CHECK(smooth == doctest::Approx(1.0f).epsilon(kLoose));
}

TEST_CASE("a smooth surface seen head on reflects its base reflectance, a rough one at a grazing angle less") {
  const Vec3 head_on = reflectance(1.0f, 0.05f);
  CHECK(head_on.x == doctest::Approx(1.0f).epsilon(kHemisphere));
  CHECK(head_on.y == doctest::Approx(0.0f).epsilon(kHemisphere));
  const Vec3 grazing = reflectance(0.1f, 1.0f);
  CHECK(grazing.x + grazing.y < head_on.x + head_on.y);
  CHECK(reflectance_table().size() ==
        static_cast<std::size_t>(kReflectanceTableSize * kReflectanceTableSize * kHalfsPerTexel));
}

TEST_CASE("light becomes half floats, never below zero nor beyond the largest") {
  CHECK(to_half(1.0f) == kHalfOne);
  CHECK(to_half(2.0f) == kHalfTwo);
  CHECK(to_half(0.0f) == 0);
  CHECK(to_half(-1.0f) == 0);
  CHECK(to_half(std::nanf("")) == 0);
  CHECK(to_half(kBeyondHalves) == kLargestHalf);
}
