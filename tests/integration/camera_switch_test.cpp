// Switching cameras in a headless game: Camera.main = and Camera.cut_to, at once and through
// black, back again, while paused, and what a new scene starts with.

#include <doctest/doctest.h>

#include <memory>
#include <string>

#include "app/presenter.hpp"
#include "physics_game.hpp"
#include "storage/save_file.hpp"
#include "test_support.hpp"

using namespace redgem;
using namespace redgem::test::physics_game;

namespace {

constexpr const char* kGame = R"({"title": "Camera switch"})";
constexpr SurfaceSize kScreen{800, 600};
constexpr int kBeforeHalfway = 29;  // frames of a one-second fade before it is half done
constexpr int kAfterHalfway = 33;   // and a few after
constexpr int kQuarterWay = 15;     // a quarter of a one-second fade
constexpr int kPastTheEnd = 64;     // a one-second fade, and a few frames more
constexpr int kFewFrames = 4;
constexpr const char* kSecurityPlace = "Vector3.new(10, 4, 0)";

// The scene's own camera first (the main one), then a security camera, and a hero.
SceneDesc two_camera_scene() {
  NodeDesc security;
  security.name = "Security";
  security.camera = CameraDesc{};
  security.local.position = {10.0f, 4.0f, 0.0f};
  NodeDesc hero;
  hero.name = "Hero";
  return scene_with({security, hero});
}

std::unique_ptr<Engine> game_on(const TempGame& game, const SceneDesc& scene, Presenter& presenter) {
  EngineOptions options;
  options.game_dir = game.dir;
  options.fixed_step = true;
  options.scene = &scene;
  options.presenter = &presenter;
  options.save_file = game.dir / "save.json";
  auto created = Engine::create(options);
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  return std::move(std::get<std::unique_ptr<Engine>>(created));
}

bool says(Engine& engine, const std::string& code) { return ruby(engine, code) == "true"; }

// Whether the game is seen through the object of that name.
bool seen_through(Engine& engine, const std::string& name) {
  return says(engine, "Camera.main == Scene.find('" + name + "')");
}

}  // namespace

TEST_CASE("Camera.main = makes another camera the one the game is seen through, and back") {
  TempGame game(kGame);
  NullPresenter screen(kScreen);
  auto engine = game_on(game, two_camera_scene(), screen);
  REQUIRE(seen_through(*engine, "Camera"));

  ruby(*engine, "Camera.main = Scene.find('Security')");
  frames(*engine, 1);
  CHECK(seen_through(*engine, "Security"));
  // What the natives see from is that camera: a ray through the screen leaves from it.
  CHECK(says(*engine, std::string("Camera.screen_ray(400, 300)[0].approx?(") + kSecurityPlace + ", 1e-4)"));

  ruby(*engine, "Camera.main = Scene.find('Camera')");
  frames(*engine, 1);
  CHECK(seen_through(*engine, "Camera"));
  ruby(*engine, "Camera.main = Scene.find('Security'); Camera.main = nil");  // nil: the scene's first
  CHECK(seen_through(*engine, "Camera"));
  CHECK(engine->scripts().error_count() == 0);
}

TEST_CASE("Camera.main = refuses what the game cannot be seen through") {
  TempGame game(kGame);
  NullPresenter screen(kScreen);
  auto engine = game_on(game, two_camera_scene(), screen);
  CHECK(error_of(*engine, "Camera.main = Scene.find('Hero')").find("holds no camera") != std::string::npos);
  CHECK(error_of(*engine, "Camera.main = 7").find("expects a GameObject") != std::string::npos);
  CHECK(error_of(*engine, "Camera.cut_to(Scene.find('Hero'), fade: 1)").find("holds no camera") != std::string::npos);
  CHECK(error_of(*engine, "Camera.cut_to(Scene.find('Security'), fade: -1)").find("fade") != std::string::npos);
  CHECK(seen_through(*engine, "Camera"));
}

TEST_CASE("a chosen camera that is destroyed hands the game back to the scene's first camera") {
  TempGame game(kGame);
  NullPresenter screen(kScreen);
  auto engine = game_on(game, two_camera_scene(), screen);
  ruby(*engine, "Camera.main = Scene.find('Security')");
  ruby(*engine, "Scene.find('Security').destroy");
  frames(*engine, 1);
  CHECK(seen_through(*engine, "Camera"));
}

TEST_CASE("Camera.cut_to without a fade cuts at once") {
  TempGame game(kGame);
  NullPresenter screen(kScreen);
  auto engine = game_on(game, two_camera_scene(), screen);
  CHECK(says(*engine, "Camera.cut_to(Scene.find('Security')) == Scene.find('Security')"));
  CHECK(seen_through(*engine, "Security"));
  CHECK(says(*engine, "!Camera.cutting?"));
}

TEST_CASE("a cut with a fade goes to black, cuts at its darkest, comes back, and cuts back") {
  TempGame game(kGame);
  NullPresenter screen(kScreen);
  auto engine = game_on(game, two_camera_scene(), screen);
  ruby(*engine, "$cut = Camera.cut_to(Scene.find('Security'), fade: 1.0).get_component(Camera::Cut)");
  CHECK(says(*engine, "Camera.cutting?"));

  frames(*engine, kQuarterWay);
  CHECK(seen_through(*engine, "Camera"));
  CHECK(says(*engine, "$cut.darkness.between?(0.4, 0.6)"));
  frames(*engine, kBeforeHalfway - kQuarterWay);
  CHECK(seen_through(*engine, "Camera"));
  frames(*engine, kAfterHalfway - kBeforeHalfway);
  CHECK(seen_through(*engine, "Security"));
  CHECK(says(*engine, "$cut.darkness > 0.8"));
  CHECK(says(*engine, "Camera.cutting?"));

  frames(*engine, kPastTheEnd - kAfterHalfway);
  CHECK(says(*engine, "!Camera.cutting? && $cut.darkness < 0.01"));
  CHECK(seen_through(*engine, "Security"));

  ruby(*engine, "Camera.cut_to(Scene.find('Camera'), fade: 1.0)");  // and back again
  frames(*engine, kPastTheEnd);
  CHECK(seen_through(*engine, "Camera"));
  CHECK(says(*engine, "!Camera.cutting?"));
  CHECK(engine->scripts().error_count() == 0);
}

TEST_CASE("a cut plays while the game is paused") {
  TempGame game(kGame);
  NullPresenter screen(kScreen);
  auto engine = game_on(game, two_camera_scene(), screen);
  ruby(*engine, "Time.scale = 0; Camera.cut_to(Scene.find('Security'), fade: 0.5)");
  frames(*engine, kOneSecond);
  CHECK(seen_through(*engine, "Security"));
  CHECK(says(*engine, "!Camera.cutting?"));
}

TEST_CASE("the newest cut wins, and Camera.main = stops a cut at once") {
  TempGame game(kGame);
  NullPresenter screen(kScreen);
  auto engine = game_on(game, two_camera_scene(), screen);
  ruby(*engine, "$first = Camera.cut_to(Scene.find('Security'), fade: 1.0).get_component(Camera::Cut)");
  frames(*engine, kFewFrames);
  ruby(*engine, "Camera.cut_to(Scene.find('Camera'), fade: 1.0)");  // the one already seen through
  frames(*engine, kPastTheEnd);
  CHECK(seen_through(*engine, "Camera"));  // the older cut never switched
  CHECK(says(*engine, "!$first.playing?"));

  ruby(*engine, "Camera.cut_to(Scene.find('Security'), fade: 1.0)");
  frames(*engine, kFewFrames);
  ruby(*engine, "Camera.main = Scene.find('Camera')");
  CHECK(says(*engine, "!Camera.cutting?"));
  frames(*engine, kPastTheEnd);
  CHECK(seen_through(*engine, "Camera"));
}

TEST_CASE("a scene loaded anew is seen through its own first camera, and no cut carries over") {
  SaveFile save;
  EngineOptions options;
  options.game_dir = test::fixture("games/sandbox");
  options.save = &save;
  options.fixed_step = true;
  auto made = Engine::create(options);
  REQUIRE(std::holds_alternative<std::unique_ptr<Engine>>(made));
  Engine& engine = *std::get<std::unique_ptr<Engine>>(made);
  ruby(engine, "$copy = Scene.spawn(Camera.main, physics: false); Camera.main = $copy");
  ruby(engine, "Camera.cut_to($copy, fade: 5)");
  frames(engine, 1);
  REQUIRE(says(engine, "Camera.main == $copy && Camera.cutting?"));
  ruby(engine, "Game.load_scene('level2')");
  frames(engine, 1);
  CHECK(says(engine, "!$copy.alive? && Camera.main.alive?"));
  CHECK(says(engine, "!Camera.cutting?"));
}

TEST_CASE("the helpers move the camera the game is seen through, and let go of the one it left") {
  TempGame game(kGame);
  NullPresenter screen(kScreen);
  auto engine = game_on(game, two_camera_scene(), screen);
  ruby(*engine, "Camera.follow(Scene.find('Hero'), offset: Vector3.new(0, 3, 6), smoothing: 0)");
  frames(*engine, 1);
  ruby(*engine, "$left = Scene.find('Camera').transform.position; Camera.main = Scene.find('Security')");
  frames(*engine, 1);
  CHECK(says(*engine, "!Camera.following?"));
  ruby(*engine, "Camera.follow(Scene.find('Hero'), offset: Vector3.new(0, 5, 5), smoothing: 0)");
  ruby(*engine, "Scene.find('Hero').transform.position = Vector3.new(2, 0, 0)");
  frames(*engine, kFewFrames);
  CHECK(says(*engine, "Scene.find('Security').transform.position.approx?(Vector3.new(2, 5, 5), 0.001)"));
  CHECK(says(*engine, "Scene.find('Camera').transform.position == $left"));
}
