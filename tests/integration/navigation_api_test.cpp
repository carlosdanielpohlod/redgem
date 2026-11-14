// The Navigation scripting API, played through Ruby in a headless game: a floor split by a wall
// with a gap, a ledge nobody climbs and a crate that moves.

#include <doctest/doctest.h>

#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <variant>

#include "physics_game.hpp"
#include "test_support.hpp"

using namespace redgem;
using namespace redgem::test::physics_game;

namespace {

constexpr const char* kChaseGame = R"({
  "title": "Chase",
  "navigation": {"cell_size": 0.5, "agent_radius": 0.3}
})";
constexpr const char* kPlainGame = R"({"title": "Plain"})";

// A game folder of the test's own holding only a game.json, removed when the test ends.
struct NavGame {
  fs::path dir = fs::temp_directory_path() / "redgem-navigation-api-test";
  explicit NavGame(const std::string& game_json) {
    fs::remove_all(dir);
    fs::create_directories(dir);
    std::string error;
    REQUIRE_MESSAGE(write_text_file(dir / "game.json", game_json, error), error);
  }
  ~NavGame() { fs::remove_all(dir); }
};

std::unique_ptr<Engine> engine_for(const NavGame& game, const SceneDesc& scene) {
  EngineOptions options;
  options.game_dir = game.dir;
  options.fixed_step = true;
  options.scene = &scene;
  options.save_file = game.dir / "save.json";
  auto created = Engine::create(options);
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  return std::move(std::get<std::unique_ptr<Engine>>(created));
}

// A 20 m floor (x and z from -10 to 10) split along x = 0 by a wall with a gap past z = 7, a ledge
// 2 m up, and a crate.
SceneDesc chase_scene() {
  SceneDesc desc;
  NodeDesc camera;
  camera.name = "Camera";
  camera.camera = CameraDesc{};
  desc.nodes = {camera, node("Ground", {0, -0.5f, 0}, {20, 1, 20}, solid()),
                node("Wall", {0, 1, -1.5f}, {0.5f, 2, 17}, solid()), node("Ledge", {7, 2, -7}, {2, 0.2f, 2}, solid()),
                node("Crate", {-5, 0.5f, -5}, {1, 1, 1}, moving("dynamic"))};
  return desc;
}

}  // namespace

TEST_CASE("Navigation.path walks around a wall, through its gap, to the target") {
  const NavGame game(kChaseGame);
  const test::LogCapture log;
  auto engine = engine_for(game, chase_scene());
  CHECK(log.contains("navigation: baked"));  // as the scene loaded: game.json has the section
  ruby(*engine, "$path = Navigation.path(Vector3.new(-5, 0, 0), Vector3.new(5, 0, 0))");
  CHECK(ruby(*engine, "$path.is_a?(Array) && $path.all? { |point| point.is_a?(Vector3) }") == "true");
  CHECK(ruby(*engine, "$path.size >= 3") == "true");
  CHECK(ruby(*engine, "$path.map(&:z).max > 7") == "true");  // through the gap
  CHECK(ruby(*engine, "($path.last - Vector3.new(5, 0, 0)).length < 0.5") == "true");
  CHECK(ruby(*engine, "($path.first - Vector3.new(-5, 0, 0)).length < 0.5") == "true");
}

TEST_CASE("Navigation.path is nil for a target nobody can walk to") {
  const NavGame game(kChaseGame);
  auto engine = engine_for(game, chase_scene());
  CHECK(ruby(*engine, "Navigation.path(Vector3.new(-5, 0, 0), Vector3.new(7, 2.1, -7)).inspect") == "nil");
  CHECK(ruby(*engine, "Navigation.path(Vector3.new(-5, 0, 0), Vector3.new(100, 0, 0)).inspect") == "nil");
}

TEST_CASE("Navigation.nearest and walkable? know where a character can stand") {
  const NavGame game(kChaseGame);
  auto engine = engine_for(game, chase_scene());
  CHECK(ruby(*engine, "Navigation.nearest(Vector3.new(0, 0, 0)).x.abs > 0.5") == "true");  // off the wall
  CHECK(ruby(*engine, "Navigation.nearest(Vector3.new(-5, 1, 0)).y.abs < 0.05") == "true");
  CHECK(ruby(*engine, "Navigation.nearest(Vector3.new(100, 0, 0)).inspect") == "nil");
  CHECK(ruby(*engine, "Navigation.walkable?(Vector3.new(-5, 0, 0))") == "true");
  CHECK(ruby(*engine, "Navigation.walkable?(Vector3.new(0, 0, 0))") == "false");
  CHECK(ruby(*engine, "Navigation.walkable?(Vector3.new(-5, 0, -5))") == "true");  // the crate moves
  CHECK(ruby(*engine, "Navigation.stats[:columns]") == "1600");
  CHECK(ruby(*engine, "Navigation.stats[:cells] > 1000") == "true");
  CHECK(ruby(*engine, "Navigation.draw(Vector3.new(0, 0, 0)).is_a?(Integer)") == "true");
}

TEST_CASE("a game without a navigation section bakes when a script first asks") {
  const NavGame game(kPlainGame);
  const test::LogCapture log;
  auto engine = engine_for(game, chase_scene());
  CHECK_FALSE(log.contains("navigation: baked"));
  CHECK(ruby(*engine, "Navigation.walkable?(Vector3.new(-5, 0, 0))") == "true");
  CHECK(log.contains("navigation: baked"));
  CHECK(ruby(*engine, "Navigation.bake[:cells] > 0") == "true");
}
