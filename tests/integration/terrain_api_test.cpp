// The Terrain scripting API, played through Ruby in a headless game: which ground layer is where
// on a terrain with ground layers, and its weights painted again by rules.

#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "physics_game.hpp"
#include "scene/ground/ground_rules.hpp"
#include "scene/ground/splat_map.hpp"
#include "scene/props.hpp"
#include "scene/shapes/terrain.hpp"
#include "test_support.hpp"

using namespace redgem;
using namespace redgem::test::physics_game;

namespace {

constexpr const char* kGame = R"({"title": "Ground"})";
constexpr TerrainGrid kGrid{20.0f, 20};
constexpr TerrainBrush kHill{0.0f, 0.0f, 6.0f, 8.0f, SculptMode::Raise};
constexpr int kSplatSize = 32;
constexpr int kTerrainNode = 2;  // after scene_with's camera and ground
constexpr Vec3 kTerrainAt{100.0f, 0.0f, 0.0f};

MeshDesc hill() {
  PrimitiveDesc ground = make_terrain(kGrid);
  sculpt_terrain(ground, kGrid, kHill);
  const Bounds bounds = mesh_bounds_of(ground);
  return MeshDesc{"Terrain", {std::move(ground)}, bounds, {}, {}};
}

// Sand everywhere, then grass on the hill's top, by the layers file's rules.
TerrainLayers sand_and_grass() {
  TerrainLayers layers;
  layers.name = "island";
  layers.node = kTerrainNode;
  for (const char* name : {"sand", "grass"}) {
    TerrainLayer layer;
    layer.name = name;
    layers.layers.push_back(layer);
  }
  GroundRule grass;
  grass.layer = 1;
  grass.height.min = 4.0f;
  layers.rules = {GroundRule{}, grass};
  layers.splat = make_splat(kSplatSize);
  return layers;
}

SceneDesc island() {
  NodeDesc terrain;
  terrain.name = "Island";
  terrain.mesh = 0;
  terrain.local.position = kTerrainAt;  // the API takes points in the world
  terrain.props[props::kTerrainLayers] = std::string("island");
  SceneDesc desc = scene_with({terrain});
  desc.meshes = {hill()};
  desc.terrains = {sand_and_grass()};
  return desc;
}

std::string at(const std::string& call, float x, float z) {
  return call + "(Scene.find('Island'), Vector3.new(" + std::to_string(kTerrainAt.x + x) + ", 0, " +
         std::to_string(kTerrainAt.z + z) + "))";
}

}  // namespace

TEST_CASE("Terrain.layers names a terrain's layers, and nil for an object without") {
  const TempGame game(kGame);
  auto engine = engine_for(game, island());
  CHECK(ruby(*engine, "Terrain.layers(Scene.find('Island')).inspect") == R"(["sand", "grass"])");
  CHECK(ruby(*engine, "Terrain.layers(Scene.find('Ground')).inspect") == "nil");
}

TEST_CASE("Terrain.layer_at and weights_at say which ground is under a point of the world") {
  const TempGame game(kGame);
  auto engine = engine_for(game, island());
  CHECK(ruby(*engine, at("Terrain.layer_at", 0, 0)) == "sand");
  CHECK(ruby(*engine, at("Terrain.weights_at", 0, 0) + "['sand']") == "1.0");
  CHECK(ruby(*engine, at("Terrain.layer_at", 0, 50) + ".inspect") == "nil");  // past its edge
}

TEST_CASE("Terrain.paint_by_rules paints the file's rules, and the next frame's weights have them") {
  const TempGame game(kGame);
  auto engine = engine_for(game, island());
  CHECK(number(*engine, "Terrain.paint_by_rules(Scene.find('Island'))") > 0.0f);
  CHECK(ruby(*engine, at("Terrain.layer_at", 0, 0)) == "grass");   // the hill's top
  CHECK(ruby(*engine, at("Terrain.layer_at", -9, -9)) == "sand");  // its foot
  frames(*engine, 1);
  CHECK(ruby(*engine, at("Terrain.layer_at", 0, 0)) == "grass");
}

TEST_CASE("Terrain.paint_by_rules takes rules from the script, as the layers file has them") {
  const TempGame game(kGame);
  auto engine = engine_for(game, island());
  ruby(*engine, "Terrain.paint_by_rules(Scene.find('Island'), [{layer: 'grass', strength: 0.5}])");
  CHECK(number(*engine, at("Terrain.weights_at", -9, -9) + "['grass']") == doctest::Approx(0.5f).epsilon(0.02));
}

TEST_CASE("Terrain.paint_by_rules refuses wrong rules, and an object without layers") {
  const TempGame game(kGame);
  auto engine = engine_for(game, island());
  CHECK(error_of(*engine, "Terrain.paint_by_rules(Scene.find('Island'), [{layer: 'lava'}])").find("names no layer") !=
        std::string::npos);
  CHECK(error_of(*engine, "Terrain.paint_by_rules(Scene.find('Ground'))").find("no ground layers") !=
        std::string::npos);
  CHECK(ruby(*engine, at("Terrain.layer_at", 0, 0)) == "sand");  // nothing painted
}

TEST_CASE("Terrain.paint_by_rules takes a seed from the script, and another seed paints other patches") {
  const TempGame game(kGame);
  auto engine = engine_for(game, island());
  const std::string ground = "g = Scene.find('Island'); ";
  const std::string noise = "noise: {min: 0.5}, noise_size: 3";
  ruby(*engine, ground + "Terrain.paint_by_rules(g, [{layer: 'grass', " + noise + ", seed: 1}])");
  const std::string first =
      ruby(*engine, "(-9..9).map { |x| Terrain.layer_at(Scene.find('Island'), [100 + x, 0, 0]) }.join(',')");
  ruby(*engine, ground + "Terrain.paint_by_rules(g, [{layer: 'sand'}, {layer: 'grass', " + noise + ", seed: 7}])");
  const std::string second =
      ruby(*engine, "(-9..9).map { |x| Terrain.layer_at(Scene.find('Island'), [100 + x, 0, 0]) }.join(',')");
  CHECK(first != second);
}
