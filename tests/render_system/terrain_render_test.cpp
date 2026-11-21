// System test: terrains drawn in a real window. A terrain without ground layers is drawn as it
// always was (a picture kept from before the layers were added); one with layers shows each where
// its weights say, blended by weight or by height, projected along three axes when asked.

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

#include "render_system/render_stage.hpp"
#include "scene/ground/splat_map.hpp"
#include "scene/props.hpp"
#include "scene/shapes/terrain.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::Picture;
using redgem::test::RenderStage;

namespace {

constexpr TerrainGrid kGrid{24.0f, 48};
constexpr Vec3 kCamera{0.0f, 11.0f, 13.0f};
constexpr redgem::Color kGrass{0.33f, 0.46f, 0.25f, 1.0f};
constexpr redgem::Color kSand{0.85f, 0.75f, 0.55f, 1.0f};
constexpr float kGroundRoughness = 0.9f;
constexpr TerrainBrush kHill{-4.0f, -2.0f, 7.0f, 3.0f, SculptMode::Raise};
constexpr TerrainBrush kBeach{5.0f, 3.0f, 6.0f, 1.0f, SculptMode::Raise};
constexpr const char* kBaseline = "images/terrain_tinted_baseline.png";
// A picture drawn again on the same card differs from its baseline by rounding at most: on
// average a fraction of a level, and no pixel by more than a few.
constexpr double kMostMeanDifference = 0.5;
constexpr int kMostPixelDifference = 6;
constexpr std::size_t kRgbChannels = 3;

MaterialDesc ground_material() {
  MaterialDesc material;
  material.name = "Ground";
  material.base_color = kGrass;
  material.roughness = kGroundRoughness;
  return material;
}

// A sculpted terrain, one hill, with a sandy patch painted into its vertex colors.
MeshDesc tinted_ground() {
  PrimitiveDesc ground = make_terrain(kGrid);
  sculpt_terrain(ground, kGrid, kHill);
  paint_terrain(ground, kGrid, kBeach, kSand);
  ground.material = 0;
  MeshDesc mesh{"Terrain", {ground}, mesh_bounds_of(ground), {}, {}};
  return mesh;
}

NodeDesc terrain_node() {
  NodeDesc node;
  node.name = "Terrain";
  node.mesh = 0;
  node.props[props::kTerrain] = true;
  node.props[props::kTerrainSize] = static_cast<double>(kGrid.size);
  node.props[props::kTerrainCells] = static_cast<double>(kGrid.cells);
  return node;
}

SceneDesc tinted_terrain() {
  SceneDesc desc;
  desc.materials = {ground_material()};
  desc.meshes = {tinted_ground()};
  desc.nodes = {test::camera_at(kCamera), terrain_node()};
  return desc;
}

struct Difference {
  double mean = 0.0;
  int most = 0;
};

Difference difference(const Picture& drawn, const Image& baseline) {
  const auto* expected = static_cast<const ::Color*>(baseline.data);
  Difference found;
  double total = 0.0;
  for (std::size_t i = 0; i < drawn.pixels.size(); ++i) {
    for (const int gap :
         {drawn.pixels[i].r - expected[i].r, drawn.pixels[i].g - expected[i].g, drawn.pixels[i].b - expected[i].b}) {
      total += std::abs(gap);
      found.most = std::max(found.most, std::abs(gap));
    }
  }
  found.mean = total / static_cast<double>(drawn.pixels.size() * kRgbChannels);
  return found;
}

}  // namespace

TEST_CASE("a terrain without ground layers is drawn as it was before layers existed") {
  RenderStage stage;
  const SceneDesc desc = tinted_terrain();
  stage.upload(desc);
  const Picture drawn = stage.draw(Scene::from_desc(desc), "terrain_tinted");
  Image baseline = LoadImage(test::fixture(kBaseline).c_str());
  REQUIRE(IsImageValid(baseline));
  ImageFormat(&baseline, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
  REQUIRE(baseline.width == drawn.width);
  REQUIRE(baseline.height == drawn.height);
  const Difference found = difference(drawn, baseline);
  UnloadImage(baseline);
  CHECK(found.mean <= kMostMeanDifference);
  CHECK(found.most <= kMostPixelDifference);
}

// --- Ground layers ------------------------------------------------------------------------------

namespace {

constexpr int kPictureSide = 64;
constexpr int kSplatSide = 64;
constexpr int kTerrainNode = 1;
constexpr int kTwoLayers = 2;
constexpr std::uint8_t kFull = 255;
constexpr std::uint8_t kMiddle = 128;
constexpr std::uint8_t kFaint = 25;
constexpr int kHeightBlock = 4;     // pixels of each square of the uneven height picture
constexpr int kStripeRows = 8;      // pixels of each stripe of the striped picture
constexpr float kRampStart = 0.3f;  // where across the splat the ramp from one layer to the next starts
constexpr float kRampEnd = 0.7f;
constexpr float kSharp = 1.0f;
constexpr float kWideTiling = 8.0f;  // meters a repeat covers on the cube
constexpr float kCubeScale = 8.0f;
constexpr int kDominance = 2;  // how many times another channel a color's own one is, to count as it
constexpr float kLeft = 0.25f;
constexpr float kRight = 0.75f;
constexpr float kCenter = 0.5f;
constexpr float kGroundRow = 0.6f;

using Rgb = std::array<std::uint8_t, kRgbChannels>;
constexpr Rgb kRed{kFull, kFaint, kFaint};
constexpr Rgb kBlue{kFaint, kFaint, kFull};
constexpr Rgb kGreen{kFaint, kFull, kFaint};
// Height of a layer's pixel: column, row.
using HeightOf = std::function<std::uint8_t(int, int)>;

std::uint8_t level_height(int /*column*/, int /*row*/) { return kMiddle; }

// Squares of random heights: low cracks and high stones (a hash of the square, its top bit).
constexpr unsigned kColumnPrime = 7919;
constexpr unsigned kRowPrime = 104729;
constexpr unsigned kHashFactor = 2654435761U;
constexpr unsigned kTopBit = 31;
std::uint8_t uneven_height(int column, int row) {
  const auto cell = static_cast<unsigned>(column / kHeightBlock) * kColumnPrime +
                    static_cast<unsigned>(row / kHeightBlock) * kRowPrime;
  return ((cell * kHashFactor) >> kTopBit) != 0 ? kFull : 0;
}

void add_layer_picture(TerrainPictures& pictures, const std::function<Rgb(int, int)>& color, const HeightOf& height) {
  for (int row = 0; row < kPictureSide; ++row) {
    for (int column = 0; column < kPictureSide; ++column) {
      const Rgb rgb = color(column, row);
      pictures.color.insert(pictures.color.end(), rgb.begin(), rgb.end());
      pictures.color.push_back(height(column, row));
      pictures.surface.insert(pictures.surface.end(), {kMiddle, kMiddle, kFull, kFull});
    }
  }
  ++pictures.layers;
}

std::function<Rgb(int, int)> flat(Rgb rgb) {
  return [rgb](int, int) { return rgb; };
}

TerrainLayers layers_of(const std::vector<Rgb>& colors) {
  TerrainLayers layers;
  layers.name = "test";
  layers.node = kTerrainNode;
  layers.pictures.side = kPictureSide;
  for (const Rgb& rgb : colors) {
    add_layer_picture(layers.pictures, flat(rgb), level_height);
    layers.layers.push_back(TerrainLayer{});
  }
  layers.splat = make_splat(kSplatSide);
  return layers;
}

// The splat's columns from `layer` on the left to `layer` + 1 on the right, by `share` of the
// way across (0 to 1).
void paint_columns(SplatMap& splat, const std::function<float(float)>& share) {
  for (int row = 0; row < splat.size; ++row) {
    for (int column = 0; column < splat.size; ++column) {
      const float right = share((static_cast<float>(column) + kCenter) / static_cast<float>(splat.size));
      set_texel(splat, column, row, LayerWeights{1.0f - right, right});
    }
  }
  ++splat.version;
}

float step_share(float across) { return across < kCenter ? 0.0f : 1.0f; }
float ramp_share(float across) { return std::clamp((across - kRampStart) / (kRampEnd - kRampStart), 0.0f, 1.0f); }

MeshDesc flat_ground() {
  PrimitiveDesc ground = make_terrain(kGrid);
  ground.material = 0;
  return MeshDesc{"Terrain", {ground}, mesh_bounds_of(ground), {}, {}};
}

SceneDesc layered_terrain(const TerrainLayers& layers) {
  SceneDesc desc;
  desc.materials = {ground_material()};
  desc.meshes = {flat_ground()};
  desc.nodes = {test::camera_at(kCamera), terrain_node()};
  desc.terrains = {layers};
  return desc;
}

bool mostly(int own, int other) { return own > other * kDominance; }
bool reddish(::Color c) { return mostly(c.r, c.b) && mostly(c.r, c.g); }
bool bluish(::Color c) { return mostly(c.b, c.r) && mostly(c.b, c.g); }
bool greenish(::Color c) { return mostly(c.g, c.r) && mostly(c.g, c.b); }

// How many pixels of the ground row, across where the ramp lies, are neither layer's color.
int mixed_pixels(const Picture& drawn) {
  int mixed = 0;
  for (int x = 0; x < drawn.width; ++x) {
    const ::Color c = drawn.at(static_cast<float>(x) / static_cast<float>(drawn.width - 1), kGroundRow);
    if (!reddish(c) && !bluish(c)) ++mixed;
  }
  return mixed;
}

Picture draw_layers(const TerrainLayers& layers, const std::string& name) {
  RenderStage stage;
  const SceneDesc desc = layered_terrain(layers);
  stage.upload(desc);
  return stage.draw(Scene::from_desc(desc), name);
}

// Red and blue, the left of the splat red and its right blue, meeting as `share` says.
TerrainLayers red_to_blue(const std::function<float(float)>& share, float sharpness) {
  TerrainLayers layers = layers_of({kRed, kBlue});
  layers.pictures.color.clear();
  layers.pictures.surface.clear();
  layers.pictures.layers = 0;
  add_layer_picture(layers.pictures, flat(kRed), uneven_height);
  add_layer_picture(layers.pictures, flat(kBlue), level_height);
  for (TerrainLayer& layer : layers.layers) layer.sharpness = sharpness;
  paint_columns(layers.splat, share);
  return layers;
}

}  // namespace

TEST_CASE("each ground layer is drawn where its weights are") {
  const Picture drawn = draw_layers(red_to_blue(step_share, 0.0f), "terrain_layers_hard_edge");
  CHECK(reddish(drawn.at(kLeft, kGroundRow)));
  CHECK(bluish(drawn.at(kRight, kGroundRow)));
}

TEST_CASE("weights that fade into each other blend the layers, and height blending sharpens the edge") {
  const int hard = mixed_pixels(draw_layers(red_to_blue(step_share, 0.0f), "terrain_layers_step"));
  const int blended = mixed_pixels(draw_layers(red_to_blue(ramp_share, 0.0f), "terrain_layers_blended"));
  const int by_height = mixed_pixels(draw_layers(red_to_blue(ramp_share, kSharp), "terrain_layers_by_height"));
  CHECK(blended > hard * kDominance);
  CHECK(blended > by_height * kDominance);
}

TEST_CASE("the second splat picture's layers are drawn") {
  TerrainLayers layers = layers_of({kRed, kRed, kRed, kRed, kRed, kGreen});
  layers.splat = make_splat(kSplatSide, static_cast<int>(layers.layers.size()) - 1);
  const Picture drawn = draw_layers(layers, "terrain_layers_sixth");
  CHECK(greenish(drawn.at(kCenter, kGroundRow)));
}

TEST_CASE("new weights reach a terrain already drawn") {
  RenderStage stage;
  SceneDesc desc = layered_terrain(layers_of({kRed, kBlue}));
  stage.upload(desc);
  CHECK(reddish(stage.draw(Scene::from_desc(desc), "terrain_layers_before").at(kCenter, kGroundRow)));
  desc.terrains[0].splat = make_splat(kSplatSide, 1);
  desc.terrains[0].splat.version = 1;
  stage.sync(desc);
  CHECK(bluish(stage.draw(Scene::from_desc(desc), "terrain_layers_after").at(kCenter, kGroundRow)));
}

namespace {

Rgb stripe(int /*column*/, int row) { return (row / kStripeRows) % kTwoLayers == 0 ? kRed : kGreen; }

// A large cube covered by one striped layer, seen from the front and above.
SceneDesc striped_cube(bool triplanar) {
  TerrainLayers layers = layers_of({});
  add_layer_picture(layers.pictures, stripe, level_height);
  TerrainLayer layer;
  layer.tiling = kWideTiling;
  layer.triplanar = triplanar;
  layers.layers = {layer};
  SceneDesc desc;
  desc.materials = {ground_material()};
  desc.meshes = {test::cube_mesh(0)};
  desc.nodes = {test::camera_at(kCamera), terrain_node()};
  desc.nodes[kTerrainNode].local.scale = Vec3{kCubeScale, kCubeScale, kCubeScale};
  desc.terrains = {layers};
  return desc;
}

// How many times the center column turns from red to green or back, top to bottom.
int stripe_turns(const Picture& drawn) {
  int turns = 0;
  bool was_red = reddish(drawn.at(kCenter, 0.0f));
  for (int y = 1; y < drawn.height; ++y) {
    const ::Color c = drawn.at(kCenter, static_cast<float>(y) / static_cast<float>(drawn.height - 1));
    const bool red = reddish(c);
    if (red == was_red || !(red || greenish(c))) continue;
    was_red = red;
    ++turns;
  }
  return turns;
}

int cube_turns(bool triplanar, const std::string& name) {
  RenderStage stage;
  const SceneDesc desc = striped_cube(triplanar);
  stage.upload(desc);
  return stripe_turns(stage.draw(Scene::from_desc(desc), name));
}

}  // namespace

TEST_CASE("a triplanar layer runs down the sides of steep ground instead of streaking") {
  const int from_above = cube_turns(false, "terrain_layers_from_above");
  const int triplanar = cube_turns(true, "terrain_layers_triplanar");
  CHECK(triplanar > from_above + kStripeRows / kTwoLayers);
}

TEST_CASE("a terrain whose mesh another object also draws gets no layers") {
  SceneDesc desc = layered_terrain(layers_of({kRed}));
  NodeDesc copy = terrain_node();
  copy.name = "Copy";
  copy.local.position = Vec3{0.0f, 0.0f, -kGrid.size};
  desc.nodes.push_back(copy);
  RenderStage stage;
  stage.upload(desc);
  CHECK_FALSE(reddish(stage.draw(Scene::from_desc(desc), "terrain_layers_shared_mesh").at(kCenter, kGroundRow)));
}
