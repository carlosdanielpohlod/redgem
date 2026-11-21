// Unit test: ground layers painted by rules (scene/ground/ground_rules.hpp) over the ground a
// terrain's mesh has under each texel (ground_survey.hpp).

#include <doctest/doctest.h>

#include <cmath>
#include <limits>
#include <string>

#include "scene/ground/ground_rules.hpp"
#include "scene/ground/splat_map.hpp"
#include "scene/ground/terrain_layers_file.hpp"
#include "scene/shapes/terrain.hpp"

using namespace redgem;
using nlohmann::json;

namespace {

constexpr TerrainGrid kGrid{20.0f, 20};
constexpr int kSplatSize = 32;
constexpr TerrainBrush kHill{0.0f, 0.0f, 6.0f, 8.0f, SculptMode::Raise};
constexpr float kClose = 1e-4f;
constexpr int kSand = 0;
constexpr int kGrass = 1;
constexpr int kRock = 2;
constexpr int kMiddle = kSplatSize / 2;
constexpr int kCorner = 0;
constexpr float kSteep = 30.0f;  // degrees

MeshDesc mesh_of(PrimitiveDesc ground) {
  const Bounds bounds = mesh_bounds_of(ground);
  return MeshDesc{"Terrain", {std::move(ground)}, bounds, {}, {}};
}

MeshDesc flat_ground() { return mesh_of(make_terrain(kGrid)); }

MeshDesc hill() {
  PrimitiveDesc ground = make_terrain(kGrid);
  sculpt_terrain(ground, kGrid, kHill);
  return mesh_of(std::move(ground));
}

TerrainLayers three_layers() {
  TerrainLayers layers;
  layers.name = "island";
  for (const char* name : {"sand", "grass", "rock"}) {
    TerrainLayer layer;
    layer.name = name;
    layers.layers.push_back(layer);
  }
  layers.splat = make_splat(kSplatSize);
  return layers;
}

std::vector<GroundRule> rules_from(const std::string& text) {
  std::vector<GroundRule> rules;
  const std::string problem = read_ground_rules(json::parse(text), three_layers(), rules);
  REQUIRE_MESSAGE(problem.empty(), problem);
  return rules;
}

std::string problem_of(const std::string& text) {
  std::vector<GroundRule> rules;
  return read_ground_rules(json::parse(text), three_layers(), rules);
}

int strongest_at(const TerrainLayers& layers, int column, int row) {
  return strongest_layer(texel_weights(layers.splat, column, row));
}

}  // namespace

TEST_CASE("a hard range takes what is inside it whole and nothing outside") {
  const SoftRange range{1.0f, 3.0f, 0.0f};
  CHECK(range_share(range, 2.0f) == 1.0f);
  CHECK(range_share(range, 1.0f) == 1.0f);
  CHECK(range_share(range, 0.5f) == 0.0f);
  CHECK(range_share(range, 3.5f) == 0.0f);
}

TEST_CASE("a soft range fades over its width, centered on each edge") {
  const SoftRange range{0.0f, 10.0f, 2.0f};
  CHECK(range_share(range, -1.0f) == 0.0f);
  CHECK(range_share(range, 0.0f) == doctest::Approx(0.5f));
  CHECK(range_share(range, 1.0f) == 1.0f);
  CHECK(range_share(range, 10.0f) == doctest::Approx(0.5f));
  CHECK(range_share(range, 11.0f) == 0.0f);
  CHECK(range_share(range, -0.5f) < range_share(range, 0.5f));
}

TEST_CASE("an open range takes everything on its open side") {
  CHECK(range_share(SoftRange{}, -1e9f) == 1.0f);
  CHECK(range_share(SoftRange{}, 1e9f) == 1.0f);
  SoftRange above;
  above.min = 5.0f;
  CHECK(range_share(above, 1e9f) == 1.0f);
  CHECK(range_share(above, 4.0f) == 0.0f);
}

TEST_CASE("the ground's noise stays from 0 to 1, the same for a place and a seed, other for another seed") {
  float lowest = 1.0f;
  float highest = 0.0f;
  int different = 0;
  for (int i = 0; i < 200; ++i) {
    const float x = static_cast<float>(i) * 0.37f;
    const float z = static_cast<float>(i) * -0.61f;
    const float noise = ground_noise(x, z, kDefaultNoiseSize, 1);
    CHECK(noise == ground_noise(x, z, kDefaultNoiseSize, 1));
    lowest = std::min(lowest, noise);
    highest = std::max(highest, noise);
    different += std::abs(noise - ground_noise(x, z, kDefaultNoiseSize, 2)) > kClose ? 1 : 0;
  }
  CHECK(lowest >= 0.0f);
  CHECK(highest <= 1.0f);
  CHECK(highest - lowest > 0.3f);
  CHECK(different > 100);
}

TEST_CASE("the survey finds the height and slope of the ground under each texel") {
  const std::vector<GroundPoint> flat = survey_ground(flat_ground(), kSplatSize);
  REQUIRE(flat.size() == static_cast<std::size_t>(kSplatSize * kSplatSize));
  CHECK(std::all_of(flat.begin(), flat.end(), [](const GroundPoint& p) { return p.covered && p.slope < kClose; }));
  const std::vector<GroundPoint> raised = survey_ground(hill(), kSplatSize);
  const GroundPoint& top = raised[static_cast<std::size_t>(kMiddle * kSplatSize + kMiddle)];
  const GroundPoint& foot = raised[static_cast<std::size_t>(kCorner)];
  CHECK(top.height > foot.height + 5.0f);
  const GroundPoint& side = raised[static_cast<std::size_t>(kMiddle * kSplatSize + kMiddle + kSplatSize / 8)];
  CHECK(side.slope > kSteep);
}

TEST_CASE("texels the mesh does not reach are not covered") {
  PrimitiveDesc triangle;
  triangle.positions = {0.0f, 0.0f, 0.0f, 10.0f, 0.0f, 0.0f, 0.0f, 0.0f, 10.0f};
  const std::vector<GroundPoint> points = survey_ground(mesh_of(triangle), kSplatSize);
  CHECK(points.front().covered);
  CHECK_FALSE(points.back().covered);  // the far corner, past the diagonal
}

TEST_CASE("rules are read with their layers, ranges, noise and strength") {
  const std::vector<GroundRule> rules = rules_from(R"([
    {"layer": "sand"},
    {"layer": "rock", "slope": {"min": 30, "soft": 10}, "height": {"max": 50},
     "noise": {"min": 0.5}, "noise_size": 12, "seed": 3, "strength": 0.8}])");
  REQUIRE(rules.size() == 2);
  CHECK(rules[0].layer == kSand);
  CHECK(std::isinf(rules[0].height.min));
  const GroundRule& rock = rules[1];
  CHECK(rock.layer == kRock);
  CHECK(rock.slope.min == 30.0f);
  CHECK(rock.slope.soft == 10.0f);
  CHECK(rock.height.max == 50.0f);
  CHECK(rock.noise.min == 0.5f);
  CHECK(rock.noise_size == 12.0f);
  CHECK(rock.seed == 3U);
  CHECK(rock.strength == doctest::Approx(0.8f));
}

TEST_CASE("rules that name no layer, or are not ranges, are refused") {
  CHECK(problem_of(R"({"layer": "sand"})").find("must be a list") != std::string::npos);
  CHECK(problem_of(R"([{"layer": "lava"}])").find("names no layer") != std::string::npos);
  CHECK(problem_of(R"([{"layer": "sand", "slope": 30}])").find("\"slope\" must be an object") != std::string::npos);
  CHECK(problem_of(R"([{"layer": "sand", "height": {"min": 5, "max": 1}}])").find("min above its max") !=
        std::string::npos);
  CHECK(problem_of(R"([7])").find("not an object") != std::string::npos);
}

TEST_CASE("a layers file reads its rules, and refuses the file when a rule is wrong") {
  const json file = json::parse(R"({"layers": [{"name": "sand"}, {"name": "grass"}],
                                    "rules": [{"layer": "grass", "height": {"min": 2}}]})");
  TerrainLayers layers;
  REQUIRE(read_terrain_layers(file, layers).empty());
  REQUIRE(layers.rules.size() == 1);
  CHECK(layers.rules[0].layer == 1);
  TerrainLayers refused;
  const json wrong = json::parse(R"({"layers": [{"name": "sand"}], "rules": [{"layer": "grass"}]})");
  CHECK_FALSE(read_terrain_layers(wrong, refused).empty());
  CHECK(refused.layers.empty());
}

TEST_CASE("rules paint each layer where the ground is in its ranges") {
  TerrainLayers layers = three_layers();
  const std::vector<GroundRule> rules = rules_from(R"([
    {"layer": "sand"},
    {"layer": "grass", "height": {"min": 4, "soft": 1}},
    {"layer": "rock", "slope": {"min": 30, "soft": 5}}])");
  const std::uint64_t version = layers.splat.version;
  CHECK(paint_ground(layers, rules, hill()) > 0);
  CHECK(layers.splat.version > version);
  CHECK(strongest_at(layers, kCorner, kCorner) == kSand);                   // low and flat
  CHECK(strongest_at(layers, kMiddle, kMiddle) == kGrass);                  // the top of the hill, flat there
  CHECK(strongest_at(layers, kMiddle + kSplatSize / 8, kMiddle) == kRock);  // its steep side
}

TEST_CASE("a rule's strength moves a texel only that far toward its layer") {
  TerrainLayers layers = three_layers();
  paint_ground(layers, rules_from(R"([{"layer": "grass", "strength": 0.25}])"), flat_ground());
  const LayerWeights weights = texel_weights(layers.splat, kMiddle, kMiddle);
  CHECK(weights[kGrass] == doctest::Approx(0.25f).epsilon(0.01));
  CHECK(weights[kSand] == doctest::Approx(0.75f).epsilon(0.01));
}

TEST_CASE("painting the same rules again changes nothing, and the version stays") {
  TerrainLayers layers = three_layers();
  const std::vector<GroundRule> rules = rules_from(R"([{"layer": "rock"}])");
  paint_ground(layers, rules, flat_ground());
  const std::uint64_t version = layers.splat.version;
  CHECK(paint_ground(layers, rules, flat_ground()) == 0);
  CHECK(layers.splat.version == version);
}

TEST_CASE("a noise rule paints patches, not everything or nothing") {
  TerrainLayers layers = three_layers();
  paint_ground(layers, rules_from(R"([{"layer": "rock", "noise": {"min": 0.5}, "noise_size": 4}])"), flat_ground());
  int rock = 0;
  for (int row = 0; row < kSplatSize; ++row) {
    for (int column = 0; column < kSplatSize; ++column) rock += strongest_at(layers, column, row) == kRock ? 1 : 0;
  }
  CHECK(rock > kSplatSize * kSplatSize / 10);
  CHECK(rock < kSplatSize * kSplatSize * 9 / 10);
}
