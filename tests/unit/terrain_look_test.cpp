// Unit test: what the lit shader is told of a terrain's ground layers.

#include <doctest/doctest.h>

#include "render/terrain_look.hpp"

using namespace redgem;

namespace {

constexpr float kTiling = 4.0f;
constexpr float kSharpness = 0.8f;
constexpr float kVariation = 0.3f;
constexpr float kVariationSize = 40.0f;
constexpr Vec3 kLow{-10.0f, 0.0f, -5.0f};
constexpr Vec3 kHigh{30.0f, 2.0f, 15.0f};

TerrainLayers two_layers() {
  TerrainLayers layers;
  layers.variation_size = kVariationSize;
  TerrainLayer rock;
  rock.tiling = kTiling;
  rock.sharpness = kSharpness;
  rock.variation = kVariation;
  rock.triplanar = true;
  TerrainLayer sand;
  sand.stochastic = true;
  layers.layers = {rock, sand};
  return layers;
}

Bounds bounds(Vec3 low, Vec3 high) {
  Bounds made;
  made.include(low);
  made.include(high);
  return made;
}

}  // namespace

TEST_CASE("a terrain's look lays its splat map over the mesh's X and Z") {
  const render_math::TerrainLook look = render_math::terrain_look(two_layers(), bounds(kLow, kHigh));
  CHECK(look.layers == 2);
  CHECK(look.area[0] == doctest::Approx(kLow.x));
  CHECK(look.area[1] == doctest::Approx(kLow.z));
  CHECK(look.area[2] == doctest::Approx(1.0f / (kHigh.x - kLow.x)));
  CHECK(look.area[3] == doctest::Approx(1.0f / (kHigh.z - kLow.z)));
}

TEST_CASE("a terrain's look tells each layer's repeats and options") {
  const render_math::TerrainLook look = render_math::terrain_look(two_layers(), bounds(kLow, kHigh));
  CHECK(look.tiling[0] == doctest::Approx(1.0f / kTiling));
  CHECK(look.sharpness[0] == doctest::Approx(kSharpness));
  CHECK(look.variation[0] == doctest::Approx(kVariation));
  CHECK(look.triplanar[0] == 1.0f);
  CHECK(look.stochastic[0] == 0.0f);
  CHECK(look.stochastic[1] == 1.0f);
  CHECK(look.variation_scale == doctest::Approx(1.0f / kVariationSize));
}

TEST_CASE("a mesh flat along X or Z gets its splat map over a meter there") {
  const render_math::TerrainLook look =
      render_math::terrain_look(two_layers(), bounds(kLow, Vec3{kLow.x, 1.0f, kLow.z}));
  CHECK(look.area[2] == doctest::Approx(1.0f));
  CHECK(look.area[3] == doctest::Approx(1.0f));
}

TEST_CASE("a terrain's look tells of no more layers than the shader holds") {
  TerrainLayers layers;
  layers.layers.resize(kMaxTerrainLayers + kMaxTerrainLayers);
  CHECK(render_math::terrain_look(layers, bounds(kLow, kHigh)).layers == kMaxTerrainLayers);
}
