// A terrain's ground layers through the game's files: its layers file, its weights saved as
// pictures and read back, and a scene that names them.

#include <raylib.h>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "app/scene_files.hpp"
#include "app/terrain_files.hpp"
#include "editor/document/scene_document.hpp"
#include "scene/ground/splat_map.hpp"
#include "scene/props.hpp"
#include "scene/scene_ids.hpp"
#include "test_support.hpp"
#include "temp_dirs.hpp"

using namespace redgem;

namespace {

constexpr int kSplatSize = 32;
constexpr int kPictureSide = 64;
constexpr int kSourceSide = 8;
constexpr TerrainGrid kGrid{20.0f, 10};
constexpr float kByteTolerance = 1.0f / kFullSplatWeight;
constexpr unsigned char kRed = 200;

struct GameFolder {
  std::filesystem::path dir = redgem::test::temp_root() / ("redgem-terrain-" + make_scene_id());
  GameFolder() { std::filesystem::create_directories(dir / "terrains"); }
  ~GameFolder() {
    std::error_code error;
    std::filesystem::remove_all(dir, error);
  }
  void write(const std::string& relative, const std::string& text) const { std::ofstream(dir / relative) << text; }
};

// A layers file of `count` layers; the first has a red color picture.
std::string layers_file(int count) {
  std::string layers = R"({"name": "layer0", "albedo": "red.png", "tiling": 2})";
  for (int i = 1; i < count; ++i) layers += R"(, {"name": "layer)" + std::to_string(i) + R"("})";
  return R"({"splat_size": )" + std::to_string(kSplatSize) + R"(, "picture_size": )" + std::to_string(kPictureSide) +
         R"(, "layers": [)" + layers + "]}";
}

void write_red_picture(const GameFolder& game) {
  const Image red = GenImageColor(kSourceSide, kSourceSide, ::Color{kRed, 0, 0, 255});
  REQUIRE(ExportImage(red, (game.dir / "red.png").string().c_str()));
  UnloadImage(red);
}

TerrainLayers read_back(const GameFolder& game, const std::string& name, TerrainReading reading) {
  std::string error;
  auto layers = read_terrain_files(game.dir, name, reading, error);
  REQUIRE_MESSAGE(layers.has_value(), error);
  return std::move(*layers);
}

LayerWeights only(int layer) {
  LayerWeights weights{};
  weights[static_cast<std::size_t>(layer)] = 1.0f;
  return weights;
}

}  // namespace

TEST_CASE("a terrain without saved weights starts all of its first layer, its pictures packed") {
  GameFolder game;
  game.write("terrains/ground.json", layers_file(4));
  write_red_picture(game);
  const TerrainLayers layers = read_back(game, "ground", TerrainReading::WeightsAndPictures);
  CHECK(layers.layers.size() == 4);
  CHECK(layers.splat.size == kSplatSize);
  CHECK(texel_weights(layers.splat, 5, 5)[0] == doctest::Approx(1.0f));
  REQUIRE(layers.pictures.layers == 4);
  CHECK(layers.pictures.side == kPictureSide);
  CHECK(layers.pictures.color[0] == kRed);  // the red picture, resized
  CHECK(layers.pictures.color[1] == 0);
  const std::size_t second = static_cast<std::size_t>(kPictureSide * kPictureSide * 4);
  CHECK(layers.pictures.color[second] == 255);        // no picture: white
  CHECK(layers.pictures.surface[second + 2] == 255);  // ... fully rough
  CHECK_FALSE(layers.pictures.has_height[0]);
  CHECK(read_back(game, "ground", TerrainReading::Weights).pictures.empty());
}

TEST_CASE("weights saved as pictures read back the same, the second picture only with more than four layers") {
  GameFolder game;
  game.write("terrains/ground.json", layers_file(6));
  TerrainLayers layers = read_back(game, "ground", TerrainReading::Weights);
  LayerWeights mixed{};
  mixed[1] = 0.25f;
  mixed[5] = 0.75f;
  set_texel(layers.splat, 3, 4, mixed);
  set_texel(layers.splat, kSplatSize - 1, kSplatSize - 1, only(4));
  std::string error;
  REQUIRE_MESSAGE(write_splat_pictures(game.dir, layers, error), error);
  CHECK(std::filesystem::exists(game.dir / "terrains/ground.splat0.png"));
  CHECK(std::filesystem::exists(game.dir / "terrains/ground.splat1.png"));
  const TerrainLayers again = read_back(game, "ground", TerrainReading::Weights);
  CHECK(again.splat.weights == layers.splat.weights);
  CHECK(texel_weights(again.splat, 3, 4)[5] == doctest::Approx(0.75f).epsilon(kByteTolerance));

  game.write("terrains/ground.json", layers_file(3));
  TerrainLayers fewer = read_back(game, "ground", TerrainReading::Weights);
  CHECK(texel_weights(fewer.splat, 3, 4)[1] == doctest::Approx(1.0f));  // layer 5's share went to the rest
  CHECK(texel_weights(fewer.splat, kSplatSize - 1, kSplatSize - 1)[0] == doctest::Approx(1.0f));
  REQUIRE(write_splat_pictures(game.dir, fewer, error));
  CHECK_FALSE(std::filesystem::exists(game.dir / "terrains/ground.splat1.png"));
}

TEST_CASE("a scene's terrain that names its layers gets them when the game reads it; a wrong name is warned of") {
  GameFolder game;
  game.write("terrains/island.json", layers_file(2));
  auto document = SceneDocument::create_empty(game.dir / "scene.glb");
  const int ground = document.add_terrain("Ground", kGrid, redgem::Color{1, 1, 1, 1});
  const int other = document.add_terrain("Other", kGrid, redgem::Color{1, 1, 1, 1});
  document.set_prop(ground, props::kTerrainLayers, std::string("island"), false);
  document.set_prop(other, props::kTerrainLayers, std::string("missing"), false);
  std::string error;
  REQUIRE_MESSAGE(document.save(error), error);

  test::LogCapture log;
  auto scene = read_game_scene(game.dir);
  REQUIRE(std::holds_alternative<SceneDesc>(scene));
  const SceneDesc& desc = std::get<SceneDesc>(scene);
  REQUIRE(desc.terrains.size() == 1);
  const int node = desc.find_node("Ground");
  REQUIRE(terrain_layers_of(desc, node) != nullptr);
  CHECK(terrain_layers_of(desc, node)->layers[1].name == "layer1");
  CHECK(terrain_layers_of(desc, desc.find_node("Other")) == nullptr);
  CHECK(log.contains("terrains/missing.json"));
}

TEST_CASE("weights saved at another size are resampled to the layers file's") {
  GameFolder game;
  game.write("terrains/ground.json", layers_file(2));
  TerrainLayers layers = read_back(game, "ground", TerrainReading::Weights);
  set_texel(layers.splat, 0, 0, only(1));
  std::string error;
  REQUIRE(write_splat_pictures(game.dir, layers, error));
  CHECK_FALSE(std::filesystem::exists(game.dir / "terrains/ground.splat0.png.tmp"));
  game.write("terrains/ground.json", R"({"splat_size": 64, "layers": [{"name": "a"}, {"name": "b"}]})");
  const TerrainLayers bigger = read_back(game, "ground", TerrainReading::Weights);
  CHECK(bigger.splat.size == 64);
  CHECK(texel_weights(bigger.splat, 0, 0)[1] == doctest::Approx(1.0f));
  CHECK(texel_weights(bigger.splat, 40, 40)[0] == doctest::Approx(1.0f));
}

TEST_CASE("a layers name that leaves the game's folder is refused") {
  GameFolder game;
  std::string error;
  CHECK_FALSE(read_terrain_files(game.dir, "../outside", TerrainReading::Weights, error).has_value());
  CHECK_FALSE(error.empty());
}
