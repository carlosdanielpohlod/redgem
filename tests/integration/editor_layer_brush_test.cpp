// The editor's Paint brush on a terrain with ground layers: it paints the chosen layer into the
// terrain's weights, undo and redo bring them back, saving writes them beside the layers file, and
// the editor opens them again as they were painted.

#include <doctest/doctest.h>

#include <filesystem>
#include <memory>
#include <string>

#include "app/terrain_files.hpp"
#include "core/files.hpp"
#include "editor/editor_session.hpp"
#include "scene/ground/splat_map.hpp"
#include "scene/props.hpp"
#include "temp_dirs.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::fixture;

namespace {

constexpr int kSplatSide = 16;
constexpr int kMiddle = kSplatSide / 2;
constexpr int kCorner = 0;
constexpr int kSand = 0;
constexpr int kGrass = 1;
constexpr float kRadius = 20.0f;  // meters: the middle texels are well inside it
constexpr float kHalfStrength = 0.5f;
constexpr const char* kBeach = "beach";
constexpr const char* kBeachLayers = R"({"splat_size": 16, "layers": [{"name": "sand"}, {"name": "grass"}]})";
// Weights far bigger than an undo step's copy of the scene's objects.
constexpr int kWideSide = 512;
constexpr const char* kWideLayers = R"({"splat_size": 512, "layers": [{"name": "sand"}, {"name": "grass"}]})";
const Vec3 kCenter{0.0f, 0.0f, 0.0f};

std::filesystem::path game_with_layers(const std::string& name, const char* layers = kBeachLayers) {
  const std::filesystem::path dir = redgem::test::temp_root() / ("redgem_layer_brush_" + name);
  std::filesystem::remove_all(dir);
  std::filesystem::copy(fixture("games/sandbox"), dir, std::filesystem::copy_options::recursive);
  std::filesystem::create_directories(dir / "terrains");
  std::string error;
  REQUIRE_MESSAGE(write_text_file(dir / "terrains" / "beach.json", layers, error), error);
  return dir;
}

std::unique_ptr<EditorSession> open(const std::filesystem::path& dir) {
  auto opened = EditorSession::open(dir);
  if (auto* error = std::get_if<std::string>(&opened)) FAIL(*error);
  return std::move(std::get<std::unique_ptr<EditorSession>>(opened));
}

// A new terrain at the middle of the world that names the beach layers.
ObjectId layered_terrain(EditorSession& session) {
  const ObjectId terrain = session.create_terrain(kCenter);
  REQUIRE(terrain != kNoObject);
  session.document().set_prop(terrain, props::kTerrainLayers, std::string(kBeach));
  session.refresh_preview();
  REQUIRE(session.terrain_layers(terrain) != nullptr);
  return terrain;
}

LayerBrush grass_brush(float strength) { return LayerBrush{0.0f, 0.0f, kRadius, strength, kHalfStrength, kGrass}; }

float grass_at(const TerrainLayers& layers, int column, int row) {
  return texel_weights(layers.splat, column, row)[kGrass];
}

float grass_in_middle(const EditorSession& session, ObjectId terrain) {
  return grass_at(*session.terrain_layers(terrain), kMiddle, kMiddle);
}

}  // namespace

TEST_CASE("the Paint brush paints a terrain's chosen layer, one undo step a stroke") {
  const auto dir = game_with_layers("stroke");
  auto session = open(dir);
  const ObjectId terrain = layered_terrain(*session);
  const auto version = session->terrain_layers(terrain)->splat.version;

  session->document().checkpoint();
  REQUIRE(session->paint_layer(terrain, kCenter, grass_brush(kHalfStrength)));
  REQUIRE(session->paint_layer(terrain, kCenter, grass_brush(kHalfStrength)));
  session->document().drop_checkpoint_if_unchanged();
  const float painted = grass_in_middle(*session, terrain);
  CHECK(painted > kHalfStrength);
  CHECK(grass_at(*session->terrain_layers(terrain), kCorner, kCorner) == 0.0f);
  CHECK(session->terrain_layers(terrain)->splat.version > version);
  CHECK(session->document().dirty());

  REQUIRE(session->document().can_undo());
  session->undo();
  CHECK(grass_in_middle(*session, terrain) == 0.0f);
  CHECK(texel_weights(session->terrain_layers(terrain)->splat, kMiddle, kMiddle)[kSand] == doctest::Approx(1.0f));
  REQUIRE(session->document().can_redo());
  session->redo();
  CHECK(grass_in_middle(*session, terrain) == doctest::Approx(painted));
  std::filesystem::remove_all(dir);
}

TEST_CASE("saving writes the painted weights, and the editor opens them as they were") {
  const auto dir = game_with_layers("save");
  float painted = 0.0f;
  std::string name;
  {
    auto session = open(dir);
    const ObjectId terrain = layered_terrain(*session);
    name = session->document().node(terrain).name;
    session->document().checkpoint();
    REQUIRE(session->paint_layer(terrain, kCenter, grass_brush(1.0f)));
    painted = grass_in_middle(*session, terrain);
    std::string error;
    REQUIRE_MESSAGE(session->save(error), error);
    CHECK_FALSE(session->document().dirty());
  }
  std::string error;
  const auto saved = read_terrain_files(dir, kBeach, TerrainReading::Weights, error);
  REQUIRE_MESSAGE(saved.has_value(), error);
  CHECK(grass_at(*saved, kMiddle, kMiddle) == doctest::Approx(painted).epsilon(0.01));

  auto again = open(dir);
  const ObjectId terrain = again->scene_desc().find_node(name);
  REQUIRE(again->terrain_layers(terrain) != nullptr);
  CHECK(grass_in_middle(*again, terrain) == doctest::Approx(painted).epsilon(0.01));
  std::filesystem::remove_all(dir);
}

TEST_CASE("the Paint brush leaves a terrain without layers to its vertex tint") {
  const auto dir = game_with_layers("plain");
  auto session = open(dir);
  const ObjectId terrain = session->create_terrain(kCenter);
  CHECK(session->terrain_layers(terrain) == nullptr);
  CHECK_FALSE(session->paint_layer(terrain, kCenter, grass_brush(1.0f)));
  std::filesystem::remove_all(dir);
}

TEST_CASE("undo steps that leave the weights alone share one copy of them") {
  const auto dir = game_with_layers("history", kWideLayers);
  auto session = open(dir);
  const ObjectId terrain = layered_terrain(*session);
  constexpr std::size_t kWeightBytes = std::size_t{kWideSide} * kWideSide * kMaxTerrainLayers;
  session->document().set_name(terrain, "Ground.a");  // (each one undo step, with the weights)
  const std::size_t one_copy = session->document().history_bytes();
  for (const char* name : {"Ground.b", "Ground.c", "Ground.d"}) session->document().set_name(terrain, name);
  CHECK(session->document().history_bytes() < one_copy + kWeightBytes);
  // A stroke changes them: the next step keeps a copy of its own.
  session->document().checkpoint();
  REQUIRE(session->paint_layer(terrain, kCenter, grass_brush(1.0f)));
  session->document().set_name(terrain, "Ground.e");
  CHECK(session->document().history_bytes() >= one_copy + kWeightBytes);
  std::filesystem::remove_all(dir);
}

TEST_CASE("a layers file that cannot be read leaves the terrain to its own material") {
  const auto dir = game_with_layers("unreadable");
  auto session = open(dir);
  const ObjectId terrain = session->create_terrain(kCenter);
  session->document().set_prop(terrain, props::kTerrainLayers, std::string("missing"));
  session->refresh_preview();
  CHECK(session->terrain_layers(terrain) == nullptr);
  CHECK(session->preview_desc().terrains.empty());
  std::filesystem::remove_all(dir);
}
