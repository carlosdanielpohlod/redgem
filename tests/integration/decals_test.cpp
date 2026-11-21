// Decals through Ruby: placed at a raycast hit, kept up to a limit, refused when wrong, and kept by
// no machine that draws nothing.

#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <utility>

#include "app/presenter.hpp"
#include "decal_meshes.hpp"
#include "integration/physics_game.hpp"
#include "scene/decals.hpp"

using namespace redgem;
using namespace redgem::test::physics_game;

namespace {

constexpr float kFloorSide = 20.0f;
constexpr int kScreenWidth = 320;
constexpr int kScreenHeight = 240;
constexpr const char* kGame = R"({"title": "Marks"})";

// A camera and a floor (a mesh that collides) at y = 0.
SceneDesc floor_scene() {
  SceneDesc desc = scene_with({});
  desc.nodes.pop_back();  // the helper's ground has no mesh to mark
  desc.meshes = {test::floor_quad(kFloorSide)};
  NodeDesc floor = node("Floor", Vec3::zero(), Vec3::one(), solid());
  floor.mesh = 0;
  desc.nodes.push_back(floor);
  return desc;
}

std::unique_ptr<Engine> drawn_engine(const TempGame& game, const SceneDesc& scene, Presenter* presenter) {
  EngineOptions options;
  options.game_dir = game.dir;
  options.fixed_step = true;
  options.scene = &scene;
  options.save_file = game.dir / "save.json";
  options.presenter = presenter;
  auto created = Engine::create(options);
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  return std::move(std::get<std::unique_ptr<Engine>>(created));
}

const DecalSystem& decals_of(Engine& engine) { return *engine.script_context().decals; }

}  // namespace

TEST_CASE("Decals.place marks the surface a raycast hit, from the hit or its point and normal") {
  const TempGame game(kGame);
  const SceneDesc scene = floor_scene();
  NullPresenter presenter({kScreenWidth, kScreenHeight});
  auto engine = drawn_engine(game, scene, &presenter);
  frames(*engine, 1);
  ruby(*engine, "$hit = Physics.raycast(Vector3.new(1, 5, 2), Vector3.new(0, -1, 0), 20)");
  REQUIRE(ruby(*engine, "$hit.nil?") == "false");
  CHECK(ruby(*engine, "Decals.place($hit)") == "true");
  CHECK(ruby(*engine, "Decals.place($hit[:point], $hit[:normal], size: 0.3, depth: 0.1, spin: 45, "
                      "color: [120, 20, 20], alpha: 200, image: :bullet_hole)") == "true");
  CHECK(ruby(*engine, "Decals.count") == "2");
  const Decal& last = decals_of(*engine).decals().back();
  CHECK(last.image == "bullet_hole");
  CHECK(last.color.a == doctest::Approx(200.0f / 255.0f));
  CHECK(ruby(*engine, "Decals.place(Vector3.new(0, 50, 0), Vector3.new(0, 1, 0))") == "false");  // in the air
  CHECK(ruby(*engine, "Decals.count") == "2");
}

TEST_CASE("Decals.limit keeps the newest marks, and Decals.clear takes them all") {
  const TempGame game(kGame);
  const SceneDesc scene = floor_scene();
  NullPresenter presenter({kScreenWidth, kScreenHeight});
  auto engine = drawn_engine(game, scene, &presenter);
  frames(*engine, 1);
  CHECK(ruby(*engine, "Decals.limit") == std::to_string(kDefaultDecalLimit));
  ruby(*engine, "Decals.limit = 3; 5.times { |i| Decals.place(Vector3.new(i, 0, 0), Vector3.up, alpha: i * 10) }");
  CHECK(ruby(*engine, "Decals.count") == "3");
  CHECK(decals_of(*engine).decals().front().color.a == doctest::Approx(20.0f / 255.0f));
  ruby(*engine, "Decals.clear");
  CHECK(ruby(*engine, "Decals.count") == "0");
  CHECK(error_of(*engine, "Decals.limit = 0").find("Decals.limit") != std::string::npos);
  CHECK(error_of(*engine, "Decals.limit = 5000").find("Decals.limit") != std::string::npos);
}

TEST_CASE("Decals.place refuses a zero normal, a size or depth that is not a positive number, and a bad color") {
  const TempGame game(kGame);
  const SceneDesc scene = floor_scene();
  NullPresenter presenter({kScreenWidth, kScreenHeight});
  auto engine = drawn_engine(game, scene, &presenter);
  frames(*engine, 1);
  CHECK(error_of(*engine, "Decals.place(Vector3.zero, Vector3.zero)").find("normal") != std::string::npos);
  for (const char* wrong : {"size: 0", "size: -1", "size: 1000", "depth: 0", "alpha: 300", "color: :plaid",
                            "spin: Float::NAN"}) {
    error_of(*engine, std::string("Decals.place(Vector3.zero, Vector3.up, ") + wrong + ")");
  }
  CHECK(ruby(*engine, "Decals.count") == "0");
}

TEST_CASE("a machine that draws nothing keeps no decals") {
  const TempGame game(kGame);
  const SceneDesc scene = floor_scene();
  auto engine = engine_for(game, scene);
  frames(*engine, 1);
  CHECK(ruby(*engine, "Decals.place(Vector3.zero, Vector3.up)") == "false");
  CHECK(ruby(*engine, "Decals.count") == "0");
}
