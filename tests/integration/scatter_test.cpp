// Scatter, as scripts use it: copies of an object's mesh put on the ground of a running game.

#include <doctest/doctest.h>

#include <filesystem>
#include <string>
#include <vector>

#include <mruby.h>
#include <mruby/string.h>

#include "app/engine.hpp"
#include "core/config.hpp"
#include "scene/props.hpp"
#include "scene/shapes/primitives.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::fixture;

namespace {

constexpr float kGroundHeight = 2.0f;  // meters: where the ground lies
constexpr float kGroundScale = 4.0f;   // of the 10 m plane: from -20 to 20 m
constexpr int kGroundMesh = 0;
constexpr int kTuftMesh = 1;
constexpr std::size_t kTuftNode = 2;
constexpr float kTolerance = 1e-3f;
constexpr const char* kOnTheGround =
    "Scatter.place('Tuft', count: 300, area: [Vector3[-10, 0, -10], Vector3[10, 0, 10]], ground: Scene.find('Ground'), "
    "seed: 3, scale: 0.5..1.5)";
// Half of this area is beyond the ground's edge.
constexpr const char* kHalfOff =
    "Scatter.place('Tuft', count: 300, area: [Vector3[0, 0, -10], Vector3[40, 0, 10]], ground: Scene.find('Ground'), "
    "seed: 3)";
constexpr std::size_t kAsked = 300;

// A camera, a ground with a collider, a tuft to scatter (a template, hidden in the game) and an
// object without a mesh.
SceneDesc meadow() {
  SceneDesc desc;
  desc.meshes = {MeshDesc{"Ground", {make_primitive(PrimitiveShape::Plane)}, {}},
                 MeshDesc{"Tuft", {make_primitive(PrimitiveShape::Cube)}, {}}};
  NodeDesc camera;
  camera.name = "Camera";
  camera.camera = CameraDesc{};
  NodeDesc ground;
  ground.name = "Ground";
  ground.mesh = kGroundMesh;
  ground.local.position = {0.0f, kGroundHeight, 0.0f};
  ground.local.scale = Vec3::one() * kGroundScale;
  ground.props = {{props::kCollider, std::string(props::kColliderMesh)}};
  NodeDesc tuft;
  tuft.name = "Tuft";
  tuft.mesh = kTuftMesh;
  tuft.props = {{props::kPrefab, true}};
  NodeDesc empty;
  empty.name = "Empty";
  desc.nodes = {camera, ground, tuft, empty};
  return desc;
}

std::unique_ptr<Engine> engine_for(const SceneDesc& scene) {
  EngineOptions options;
  options.game_dir = fixture("games/sandbox");
  options.fixed_step = true;
  options.scene = &scene;
  options.save_file = std::filesystem::temp_directory_path() / "redgem-scatter-test" / "save.json";  // never written
  auto created = Engine::create(options);
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  auto engine = std::move(std::get<std::unique_ptr<Engine>>(created));
  engine->frame(config::kFixedDeltaSeconds);
  return engine;
}

std::string ruby(Engine& engine, const std::string& code) {
  const RubyResult result = engine.scripts().eval(code, "test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  mrb_state* mrb = engine.scripts().vm().state();
  return RSTRING_PTR(mrb_obj_as_string(mrb, result.value));
}

bool fails(Engine& engine, const std::string& code) { return !engine.scripts().eval(code, "bad.rb").ok; }

const std::vector<Transform>& places(Engine& engine, std::size_t set) {
  return engine.scene().scattered().sets().at(set).places;
}

}  // namespace

TEST_CASE("Scatter.place puts copies of an object's mesh on the ground, and says how many") {
  const SceneDesc scene = meadow();
  auto engine = engine_for(scene);
  CHECK(ruby(*engine, std::string("$meadow = ") + kOnTheGround + "; $meadow.count") == std::to_string(kAsked));
  REQUIRE(engine->scene().scattered().sets().size() == 1);
  const ScatterSet& set = engine->scene().scattered().sets().front();
  CHECK(set.mesh == kTuftMesh);
  CHECK(set.places.size() == kAsked);
  for (const Transform& place : set.places) {
    CHECK(place.position.y == doctest::Approx(kGroundHeight).epsilon(kTolerance));
    CHECK(place.scale.x >= 0.5f);
    CHECK(place.scale.x <= 1.5f);
  }
  CHECK(engine->scene().at(static_cast<ObjectId>(kTuftNode)).prefab);  // the template stays a template
}

TEST_CASE("the same seed scatters the same copies; where the ground ends none are put") {
  const SceneDesc scene = meadow();
  auto engine = engine_for(scene);
  ruby(*engine, kOnTheGround);
  ruby(*engine, kOnTheGround);
  REQUIRE(engine->scene().scattered().sets().size() == 2);
  for (std::size_t i = 0; i < kAsked; ++i) CHECK(places(*engine, 0)[i].position.x == places(*engine, 1)[i].position.x);
  const std::string placed = ruby(*engine, std::string(kHalfOff) + ".count");
  CHECK(std::stoul(placed) < kAsked);
  CHECK(std::stoul(placed) > 0);
  for (const Transform& place : places(*engine, 2)) CHECK(place.position.x <= kGroundScale * 5.0f);
}

TEST_CASE("scattered copies are taken away by their set, or all at once") {
  const SceneDesc scene = meadow();
  auto engine = engine_for(scene);
  CHECK(ruby(*engine, std::string("first = ") + kOnTheGround + "; " + kOnTheGround + "; first.remove") == "true");
  CHECK(engine->scene().scattered().sets().size() == 1);
  CHECK(ruby(*engine, "Scatter.clear; 'cleared'") == "cleared");
  CHECK(engine->scene().scattered().sets().empty());
}

TEST_CASE("Scatter.place refuses what it cannot scatter, and numbers out of their range") {
  const SceneDesc scene = meadow();
  auto engine = engine_for(scene);
  CHECK(fails(*engine, "Scatter.place('Nobody', count: 1, area: [Vector3.zero, Vector3.one])"));
  CHECK(fails(*engine, "Scatter.place('Empty', count: 1, area: [Vector3.zero, Vector3.one])"));
  CHECK(fails(*engine, "Scatter.place('Tuft', count: 0, area: [Vector3.zero, Vector3.one])"));
  CHECK(fails(*engine, "Scatter.place('Tuft', count: 1, area: [Vector3.zero, Vector3.one], scale: 2.0..1.0)"));
  CHECK(fails(*engine, "Scatter.place('Tuft', count: 1, area: [Vector3.zero, Vector3.one], seed: -1)"));
  CHECK(engine->scene().scattered().sets().empty());
}

TEST_CASE("without a ground, copies stand on the bottom of their area") {
  const SceneDesc scene = meadow();
  auto engine = engine_for(scene);
  ruby(*engine, "Scatter.place('Tuft', count: 10, area: [Vector3[-1, 5, -1], Vector3[1, 9, 1]])");
  for (const Transform& place : places(*engine, 0)) CHECK(place.position.y == 5.0f);
}
