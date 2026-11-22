// Unit test: the soft round brush that paints a ground layer into a terrain's weights
// (scene/ground/ground_brush.hpp).

#include <doctest/doctest.h>

#include "scene/ground/ground_brush.hpp"
#include "scene/ground/splat_map.hpp"

using namespace redgem;

namespace {

constexpr int kSplatSize = 32;
constexpr float kHalfSide = 10.0f;  // meters: the mesh spans -10..10 along X and Z
constexpr float kClose = 1e-4f;
constexpr int kSand = 0;
constexpr int kGrass = 1;
constexpr int kMissingLayer = 2;  // the terrain has two
constexpr int kMiddle = kSplatSize / 2;
constexpr int kCorner = 0;
constexpr float kRadius = 4.0f;
constexpr float kHalfStrength = 0.5f;
constexpr float kOneSecond = 1.0f;
constexpr float kHalfSecond = 0.5f;
constexpr float kQuarterTurn = 0.25f;

const Bounds kBounds{{-kHalfSide, 0.0f, -kHalfSide}, {kHalfSide, 0.0f, kHalfSide}};

TerrainLayers two_layers() {
  TerrainLayers layers;
  layers.name = "island";
  for (const char* name : {"sand", "grass"}) {
    TerrainLayer layer;
    layer.name = name;
    layers.layers.push_back(layer);
  }
  layers.splat = make_splat(kSplatSize);
  return layers;
}

LayerBrush grass_brush(float strength, float falloff) {
  return LayerBrush{0.0f, 0.0f, kRadius, strength, falloff, kGrass};
}

float grass_at(const TerrainLayers& layers, int column, int row) {
  return texel_weights(layers.splat, column, row)[kGrass];
}

}  // namespace

TEST_CASE("a brush paints its strength inside and fades out softly at its rim") {
  const LayerBrush brush = grass_brush(1.0f, kHalfStrength);
  CHECK(brush_share(brush, 0.0f) == doctest::Approx(1.0f));
  CHECK(brush_share(brush, kRadius * kHalfStrength) == doctest::Approx(1.0f));
  CHECK(brush_share(brush, kRadius * (1.0f - kQuarterTurn)) == doctest::Approx(kHalfStrength));
  CHECK(brush_share(brush, kRadius) == 0.0f);
  CHECK(brush_share(brush, kRadius * 2.0f) == 0.0f);
}

TEST_CASE("a brush without falloff has a hard edge, and one of full falloff fades from its middle") {
  const LayerBrush hard = grass_brush(kHalfStrength, 0.0f);
  CHECK(brush_share(hard, kRadius - kClose) == doctest::Approx(kHalfStrength));
  const LayerBrush soft = grass_brush(1.0f, 1.0f);
  CHECK(brush_share(soft, kRadius * kHalfStrength) == doctest::Approx(kHalfStrength));
}

TEST_CASE("a held brush goes as far in a second at any frame rate") {
  const float once = strength_for(kHalfStrength, kOneSecond);
  const float half = strength_for(kHalfStrength, kHalfSecond);
  CHECK(once == doctest::Approx(kHalfStrength));
  CHECK(1.0f - (1.0f - half) * (1.0f - half) == doctest::Approx(once));
  CHECK(strength_for(1.0f, kHalfSecond) == doctest::Approx(1.0f));
  CHECK(strength_for(kHalfStrength, 0.0f) == 0.0f);
}

TEST_CASE("painting a layer moves the texels under the brush toward it and leaves the rest") {
  TerrainLayers layers = two_layers();
  const auto version = layers.splat.version;
  const std::size_t changed = brush_layer(layers, kBounds, grass_brush(1.0f, kHalfStrength));
  CHECK(changed > 0);
  CHECK(layers.splat.version == version + 1);
  CHECK(grass_at(layers, kMiddle, kMiddle) == doctest::Approx(1.0f));
  CHECK(grass_at(layers, kCorner, kCorner) == 0.0f);
  const LayerWeights middle = texel_weights(layers.splat, kMiddle, kMiddle);
  CHECK(middle[kSand] + middle[kGrass] == doctest::Approx(1.0f));
}

TEST_CASE("a weaker brush goes part of the way, and strokes add up") {
  TerrainLayers layers = two_layers();
  const LayerBrush brush = grass_brush(kHalfStrength, kHalfStrength);
  brush_layer(layers, kBounds, brush);
  const float once = grass_at(layers, kMiddle, kMiddle);
  CHECK(once == doctest::Approx(kHalfStrength).epsilon(0.01));
  brush_layer(layers, kBounds, brush);
  CHECK(grass_at(layers, kMiddle, kMiddle) > once);
}

TEST_CASE("a brush that changes nothing keeps the version, and one of no layer paints nothing") {
  TerrainLayers layers = two_layers();
  const auto version = layers.splat.version;
  CHECK(brush_layer(layers, kBounds, LayerBrush{0.0f, 0.0f, kRadius, 1.0f, 0.0f, kSand}) == 0);
  CHECK(brush_layer(layers, kBounds, LayerBrush{0.0f, 0.0f, kRadius, 1.0f, 0.0f, kMissingLayer}) == 0);
  CHECK(brush_layer(layers, kBounds, LayerBrush{kHalfSide * 4.0f, 0.0f, kRadius, 1.0f, 0.0f, kGrass}) == 0);
  CHECK(layers.splat.version == version);
}
