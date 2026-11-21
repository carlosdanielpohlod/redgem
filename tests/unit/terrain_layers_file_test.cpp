#include <doctest/doctest.h>

#include "scene/ground/terrain_layers_file.hpp"

using namespace redgem;
using nlohmann::json;

namespace {

json two_layers() {
  return json::parse(R"({
    "splat_size": 512, "picture_size": 256, "variation_size": 25,
    "layers": [
      {"name": "sand", "albedo": "textures/sand/Color.jpg", "height": "textures/sand/Height.png",
       "tiling": 3, "sharpness": 0.8, "variation": 0.3, "stochastic": true, "tint": "#ffffff"},
      {"name": "rock", "normal": "textures/rock/NormalGL.jpg", "triplanar": true, "tint": [0.5, 0.25, 1]}
    ]})");
}

}  // namespace

TEST_CASE("a layers file gives each layer its pictures and looks, and the defaults for what it leaves out") {
  TerrainLayers layers;
  REQUIRE(read_terrain_layers(two_layers(), layers).empty());
  REQUIRE(layers.layers.size() == 2);
  CHECK(layers.splat_size == 512);
  CHECK(layers.picture_size == 256);
  CHECK(layers.variation_size == doctest::Approx(25.0f));
  const TerrainLayer& sand = layers.layers[0];
  CHECK(sand.albedo == "textures/sand/Color.jpg");
  CHECK(sand.height == "textures/sand/Height.png");
  CHECK(sand.normal.empty());
  CHECK(sand.tiling == doctest::Approx(3.0f));
  CHECK(sand.sharpness == doctest::Approx(0.8f));
  CHECK(sand.stochastic);
  CHECK_FALSE(sand.triplanar);
  CHECK(sand.tint.x == doctest::Approx(1.0f));
  const TerrainLayer& rock = layers.layers[1];
  CHECK(rock.triplanar);
  CHECK(rock.tiling == doctest::Approx(kDefaultLayerTiling));
  CHECK(rock.sharpness == doctest::Approx(kDefaultLayerSharpness));
  CHECK(rock.tint.y == doctest::Approx(0.25f));
  CHECK(layer_named(layers, "rock") == 1);
  CHECK(layer_named(layers, "snow") == kNoLayer);
}

TEST_CASE("numbers past their bounds are clamped") {
  TerrainLayers layers;
  const json file = json::parse(R"({"splat_size": 99999, "picture_size": 1,
    "layers": [{"name": "a", "tiling": -2, "sharpness": 7, "variation": -1}]})");
  REQUIRE(read_terrain_layers(file, layers).empty());
  CHECK(layers.splat_size == kMaxSplatSize);
  CHECK(layers.picture_size == kMinLayerPictureSize);
  CHECK(layers.layers[0].tiling > 0.0f);
  CHECK(layers.layers[0].sharpness == doctest::Approx(1.0f));
  CHECK(layers.layers[0].variation == doctest::Approx(0.0f));
}

TEST_CASE("a layers file that cannot be used says why and changes nothing") {
  TerrainLayers layers;
  layers.splat_size = kMinSplatSize;
  CHECK_FALSE(read_terrain_layers(json::array(), layers).empty());
  CHECK_FALSE(read_terrain_layers(json::parse(R"({"layers": []})"), layers).empty());
  CHECK_FALSE(read_terrain_layers(json::parse(R"({"layers": [{"albedo": "a.png"}]})"), layers).empty());
  CHECK_FALSE(read_terrain_layers(json::parse(R"({"layers": [{"name": "a"}, {"name": "a"}]})"), layers).empty());
  CHECK_FALSE(read_terrain_layers(json::parse(R"({"layers": [{"name": "a", "tint": "red"}]})"), layers).empty());
  json nine = json::object({{"layers", json::array()}});
  for (int i = 0; i <= kMaxTerrainLayers; ++i) nine["layers"].push_back({{"name", std::to_string(i)}});
  CHECK(read_terrain_layers(nine, layers).find("1 to 8") != std::string::npos);
  CHECK(layers.layers.empty());
  CHECK(layers.splat_size == kMinSplatSize);
}

TEST_CASE("a terrain's files are in the terrains folder, named after it") {
  CHECK(terrain_layers_path("island") == "terrains/island.json");
  CHECK(splat_picture_path("island", 0) == "terrains/island.splat0.png");
  CHECK(splat_picture_path("island", 1) == "terrains/island.splat1.png");
}
