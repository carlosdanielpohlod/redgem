#include <doctest/doctest.h>

#include <limits>
#include <numeric>

#include "scene/ground/splat_map.hpp"

using namespace redgem;

namespace {

constexpr int kSize = 16;
constexpr float kTolerance = 1e-4f;
constexpr float kByteTolerance = 1.0f / kFullSplatWeight;
constexpr int kSand = 0;
constexpr int kGrass = 1;
constexpr int kRock = 5;  // in the second picture

int byte_total(const std::array<std::uint8_t, kMaxTerrainLayers>& bytes) {
  return std::accumulate(bytes.begin(), bytes.end(), 0);
}

float share_total(const LayerWeights& weights) { return std::accumulate(weights.begin(), weights.end(), 0.0f); }

LayerWeights only(int layer) {
  LayerWeights weights{};
  weights[static_cast<std::size_t>(layer)] = 1.0f;
  return weights;
}

}  // namespace

TEST_CASE("a new splat map is covered by one layer, and its size is kept within bounds") {
  const SplatMap splat = make_splat(kSize, kGrass);
  CHECK(well_formed(splat));
  CHECK(splat.weights.size() == static_cast<std::size_t>(kSize * kSize * kMaxTerrainLayers));
  CHECK(texel_weights(splat, 3, 7)[kGrass] == doctest::Approx(1.0f));
  CHECK(texel_weights(splat, 3, 7)[kSand] == doctest::Approx(0.0f));
  CHECK(make_splat(1).size == kMinSplatSize);
  CHECK(make_splat(kMaxSplatSize * 2).size == kMaxSplatSize);
  CHECK_FALSE(well_formed(SplatMap{}));
}

TEST_CASE("shares are normalized: bad ones count as nothing, and nothing at all is the first layer") {
  LayerWeights weights{};
  weights[kSand] = 3.0f;
  weights[kGrass] = 1.0f;
  weights[2] = -4.0f;
  weights[3] = std::numeric_limits<float>::quiet_NaN();
  weights[4] = std::numeric_limits<float>::infinity();
  const LayerWeights result = normalized(weights);
  CHECK(result[kSand] == doctest::Approx(0.75f));
  CHECK(result[kGrass] == doctest::Approx(0.25f));
  CHECK(result[2] == 0.0f);
  CHECK(result[3] == 0.0f);
  CHECK(result[4] == 0.0f);
  CHECK(normalized(LayerWeights{})[kSand] == doctest::Approx(1.0f));
}

TEST_CASE("quantized weights always add up to a whole byte") {
  const LayerWeights thirds{1.0f, 1.0f, 1.0f};
  CHECK(byte_total(quantized(thirds)) == kFullSplatWeight);
  const LayerWeights eighths{1, 1, 1, 1, 1, 1, 1, 1};
  const auto bytes = quantized(eighths);
  CHECK(byte_total(bytes) == kFullSplatWeight);
  for (const std::uint8_t byte : bytes) CHECK((byte == 31 || byte == 32));
  CHECK(byte_total(quantized(LayerWeights{})) == kFullSplatWeight);
}

TEST_CASE("a texel written and read back keeps its shares, to a byte") {
  SplatMap splat = make_splat(kSize);
  LayerWeights weights{};
  weights[kSand] = 0.2f;
  weights[kRock] = 0.8f;
  set_texel(splat, 4, 9, weights);
  const LayerWeights read = texel_weights(splat, 4, 9);
  CHECK(read[kSand] == doctest::Approx(0.2f).epsilon(kByteTolerance));
  CHECK(read[kRock] == doctest::Approx(0.8f).epsilon(kByteTolerance));
  CHECK(share_total(read) == doctest::Approx(1.0f).epsilon(kTolerance));
  set_texel(splat, kSize, 0, only(kRock));  // outside: nothing happens
  CHECK(texel_weights(splat, kSize - 1, 0)[kSand] == doctest::Approx(1.0f));
}

TEST_CASE("weights between texels are mixed from the four around the spot") {
  SplatMap splat = make_splat(kMinSplatSize, kSand);
  for (int row = 0; row < splat.size; ++row) {
    for (int column = splat.size / 2; column < splat.size; ++column) set_texel(splat, column, row, only(kGrass));
  }
  const LayerWeights middle = splat_weights_at(splat, 0.5f, 0.5f);
  CHECK(middle[kSand] == doctest::Approx(0.5f).epsilon(kByteTolerance));
  CHECK(middle[kGrass] == doctest::Approx(0.5f).epsilon(kByteTolerance));
  CHECK(splat_weights_at(splat, 0.0f, 0.3f)[kSand] == doctest::Approx(1.0f));
  CHECK(splat_weights_at(splat, 2.0f, 0.3f)[kGrass] == doctest::Approx(1.0f));  // clamped to the edge
  CHECK(share_total(splat_weights_at(splat, 0.47f, 0.81f)) == doctest::Approx(1.0f).epsilon(kTolerance));
}

TEST_CASE("moving toward a layer keeps the shares whole, and goes all the way at a full share") {
  const LayerWeights start = only(kSand);
  const LayerWeights half = toward_layer(start, kGrass, 0.5f);
  CHECK(half[kSand] == doctest::Approx(0.5f));
  CHECK(half[kGrass] == doctest::Approx(0.5f));
  CHECK(toward_layer(half, kGrass, 1.0f)[kGrass] == doctest::Approx(1.0f));
  CHECK(toward_layer(half, kGrass, 4.0f)[kGrass] == doctest::Approx(1.0f));            // clamped
  CHECK(toward_layer(half, kMaxTerrainLayers, 1.0f)[kSand] == doctest::Approx(0.5f));  // no such layer
  CHECK(strongest_layer(toward_layer(start, kRock, 0.6f)) == kRock);
}

TEST_CASE("a map goes into two RGBA pictures and comes back the same") {
  SplatMap splat = make_splat(kSize);
  LayerWeights weights{};
  weights[kGrass] = 0.4f;
  weights[kRock] = 0.6f;
  set_texel(splat, 1, 2, weights);
  const auto first = splat_picture(splat, 0);
  const auto second = splat_picture(splat, 1);
  CHECK(first.size() == static_cast<std::size_t>(kSize * kSize * kLayersPerSplatPicture));
  SplatMap copy = make_splat(kSize);
  REQUIRE(take_splat_picture(copy, 0, first));
  REQUIRE(take_splat_picture(copy, 1, second));
  CHECK(copy.weights == splat.weights);
  CHECK_FALSE(take_splat_picture(copy, 0, std::vector<std::uint8_t>(3)));
}

TEST_CASE("renormalizing a hand-edited map makes every texel whole again") {
  SplatMap splat = make_splat(kSize);
  splat.weights.assign(splat.weights.size(), 0);  // a texel of nothing goes to the first layer
  splat.weights[kGrass] = 10;                     // the first texel: a little grass only
  renormalize(splat);
  CHECK(texel_weights(splat, 0, 0)[kGrass] == doctest::Approx(1.0f));
  CHECK(texel_weights(splat, 1, 0)[kSand] == doctest::Approx(1.0f));
  for (std::size_t at = 0; at < splat.weights.size(); at += kMaxTerrainLayers) {
    int total = 0;
    for (int layer = 0; layer < kMaxTerrainLayers; ++layer)
      total += splat.weights[at + static_cast<std::size_t>(layer)];
    REQUIRE(total == kFullSplatWeight);
  }
}
