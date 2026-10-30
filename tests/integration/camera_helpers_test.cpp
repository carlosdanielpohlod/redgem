// The camera helpers played in a headless game: Camera.follow, orbit, shake and world_to_screen,
// with a target that goes away, a camera on a moving parent, walls in the way, other screens.

#include <doctest/doctest.h>

#include <memory>
#include <string>

#include "app/presenter.hpp"
#include "input/devices.hpp"
#include "physics_game.hpp"

using namespace redgem;
using namespace redgem::test::physics_game;

namespace {

constexpr const char* kGame = R"({"title": "Camera helpers"})";
constexpr SurfaceSize kScreen{800, 600};
constexpr SurfaceSize kOddScreen{333, 177};
constexpr int kHalfSecond = kOneSecond / 2;
constexpr int kFewFrames = 10;
constexpr float kMouseStart = 100.0f;
constexpr float kDragged = 100.0f;  // pixels: 30 degrees at the orbit's 0.3 degrees a pixel
constexpr float kWheelNotches = 2.0f;

// A hero at the origin, and a camera on a dolly: the dolly's place and turn are what a camera with
// a parent gets wrong when it mixes its own space with the world's.
SceneDesc parented_scene() {
  NodeDesc dolly;
  dolly.name = "Dolly";
  dolly.local.position = {5.0f, 2.0f, 5.0f};
  dolly.local.rotation = quat_from_euler_degrees({0.0f, 45.0f, 0.0f});
  dolly.children = {1};
  NodeDesc camera;
  camera.name = "Camera";
  camera.camera = CameraDesc{};
  camera.parent = 0;
  camera.local.position = {0.0f, 1.0f, 2.0f};
  NodeDesc hero;
  hero.name = "Hero";
  SceneDesc desc;
  desc.nodes = {dolly, camera, hero};
  return desc;
}

SceneDesc hero_scene(std::vector<NodeDesc> more = {}) {
  NodeDesc hero;
  hero.name = "Hero";
  NodeDesc other;
  other.name = "Other";
  more.push_back(hero);
  more.push_back(other);
  return scene_with(std::move(more));
}

// A hero carrying a shield (a collider of its own) between it and where the camera follows it.
SceneDesc shielded_scene() {
  NodeDesc hero;
  hero.name = "Hero";
  hero.children = {3};
  NodeDesc shield = node("Shield", {0.0f, 1.0f, 1.0f}, {2.0f, 2.0f, 0.2f}, solid());
  shield.parent = 2;  // (after the camera and the ground)
  return scene_with({hero, shield});
}

// A camera, a hero and a crate. Slots of objects made while the game runs are taken again by the
// next copy, and the first camera in the scene's order is the main one: so a copy of the camera
// made where a copy of the crate was comes before a copy made after it, and takes over.
SceneDesc copied_camera_scene() {
  NodeDesc hero;
  hero.name = "Hero";
  NodeDesc camera;
  camera.name = "Camera";
  camera.camera = CameraDesc{};
  NodeDesc crate;
  crate.name = "Crate";
  SceneDesc desc;
  desc.nodes = {hero, camera, crate};
  return desc;
}

// A screen 800 x 400 showing the world drawn square, between bars: from x = 200 to 600.
class SquarePicture final : public Presenter {
 public:
  SurfaceSize surface_size() const override { return {800, 400}; }
  SurfaceSize picture_size() const override { return {400, 400}; }
  int text_width(const std::string& text, int) const override { return static_cast<int>(text.size()); }
  void capture_cursor(bool) override {}
  void present(const FrameView&) override {}
  bool open() const override { return true; }
  float frame_seconds() const override { return config::kFixedDeltaSeconds; }
};

std::unique_ptr<Engine> game_on(const TempGame& game, const SceneDesc& scene, Presenter& presenter) {
  EngineOptions options;
  options.game_dir = game.dir;
  options.fixed_step = true;
  options.random_seed = 1;
  options.scene = &scene;
  options.presenter = &presenter;
  options.save_file = game.dir / "save.json";
  auto created = Engine::create(options);
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  return std::move(std::get<std::unique_ptr<Engine>>(created));
}

// Whether the camera sees from its own place: no shake moves what it shows.
constexpr const char* kSeesFromItsPlace =
    "ray = Camera.screen_ray(400, 300); ray[0].approx?(Camera.main.transform.position, 1e-5) && "
    "ray[1].approx?(Camera.main.transform.forward, 1e-5)";

bool says(Engine& engine, const std::string& code) { return ruby(engine, code) == "true"; }

bool fails(Engine& engine, const std::string& code) { return !engine.scripts().eval(code, "test.rb").ok; }

}  // namespace

TEST_CASE("Camera.follow brings the camera to its offset from the target and looks at it") {
  TempGame game(kGame);
  NullPresenter screen(kScreen);
  auto engine = game_on(game, hero_scene(), screen);
  ruby(*engine, "Camera.follow(Scene.find('Hero'), offset: Vector3.new(0, 3, 6), smoothing: 0.1)");
  frames(*engine, kTwoSeconds);
  CHECK(says(*engine, "Camera.main.transform.position.approx?(Vector3.new(0, 3, 6), 0.01)"));
  CHECK(says(*engine,
             "Camera.main.transform.forward.dot((Vector3.up - Camera.main.transform.position).normalized) > 0.999"));
  CHECK(says(*engine, "Camera.following?"));

  ruby(*engine, "Scene.find('Hero').transform.position = Vector3.new(4, 0, -2)");
  frames(*engine, kTwoSeconds);
  CHECK(says(*engine, "Camera.main.transform.position.approx?(Vector3.new(4, 3, 4), 0.01)"));

  // In the target's own axes, the camera turns with it: behind a hero facing +X is its -X side.
  ruby(*engine, "Scene.find('Hero').transform.rotation = Vector3.new(0, -90, 0)");
  ruby(*engine, "Camera.follow(Scene.find('Hero'), offset: Vector3.new(0, 3, 6), relative: true, smoothing: 0)");
  frames(*engine, 1);
  CHECK(says(*engine, "Camera.main.transform.position.approx?(Vector3.new(-2, 3, -2), 0.01)"));

  ruby(*engine, "Camera.stop_following");
  ruby(*engine, "Scene.find('Hero').transform.position = Vector3.zero");
  frames(*engine, kFewFrames);
  CHECK(says(*engine, "Camera.main.transform.position.approx?(Vector3.new(-2, 3, -2), 0.01)"));
  CHECK(says(*engine, "!Camera.following?"));
  CHECK(engine->scripts().error_count() == 0);
}

TEST_CASE("the dead zone lets the target move a little before the camera goes after it") {
  TempGame game(kGame);
  NullPresenter screen(kScreen);
  auto engine = game_on(game, hero_scene(), screen);
  ruby(*engine, "Camera.follow(Scene.find('Hero'), offset: Vector3.new(0, 3, 6), smoothing: 0, dead_zone: 1.0)");
  frames(*engine, 1);
  ruby(*engine, "Scene.find('Hero').transform.position = Vector3.new(0.5, 0, 0)");
  frames(*engine, kFewFrames);
  CHECK(says(*engine, "Camera.main.transform.position.approx?(Vector3.new(0, 3, 6), 0.001)"));
  ruby(*engine, "Scene.find('Hero').transform.position = Vector3.new(3, 0, 0)");
  frames(*engine, 1);
  CHECK(says(*engine, "Camera.main.transform.position.approx?(Vector3.new(2, 3, 6), 0.001)"));
}

TEST_CASE("a target destroyed while followed or orbited is let go, and the camera stays where it was") {
  TempGame game(kGame);
  NullPresenter screen(kScreen);
  auto engine = game_on(game, hero_scene(), screen);
  ruby(*engine, "Camera.follow(Scene.find('Hero'), smoothing: 0.1)");
  frames(*engine, kOneSecond);
  ruby(*engine, "$before = Camera.main.transform.position; Scene.find('Hero').destroy");
  frames(*engine, kFewFrames);
  CHECK(says(*engine, "Camera.main.transform.position == $before"));
  CHECK(says(*engine, "!Camera.following?"));

  ruby(*engine, "Camera.orbit(Scene.find('Other'), distance: 4, smoothing: 0.1)");
  frames(*engine, kHalfSecond);
  ruby(*engine, "$before = Camera.main.transform.position; Scene.find('Other').destroy");
  frames(*engine, kFewFrames);
  CHECK(says(*engine, "Camera.main.transform.position == $before"));
  CHECK(says(*engine, "!Camera.following?"));
  CHECK(engine->scripts().error_count() == 0);
  // (One that is gone already cannot be followed.)
  CHECK(fails(*engine, "Camera.follow(Scene.find('Nobody'))"));
}

TEST_CASE("a camera with a parent follows in the world's space, and is seen from its own place after a shake") {
  TempGame game(kGame);
  NullPresenter screen(kScreen);
  auto engine = game_on(game, parented_scene(), screen);
  ruby(*engine, "Camera.follow(Scene.find('Hero'), offset: Vector3.new(0, 3, 6), smoothing: 0.05)");
  frames(*engine, kOneSecond);
  CHECK(says(*engine, "Camera.main.transform.position.approx?(Vector3.new(0, 3, 6), 0.01)"));
  CHECK(says(*engine, "Camera.world_to_screen(Vector3.up).zip([400, 300]).all? { |a, b| (a - b).abs < 0.5 }"));

  // The dolly carries the camera while it shakes; the shake over, it sees from where the dolly took it.
  ruby(*engine, "Camera.stop_following; $local = Camera.main.transform.local_position");
  ruby(*engine, "Camera.shake(1.0, duration: 0.5)");
  for (int frame = 0; frame < kOneSecond; ++frame) {
    ruby(*engine, "Scene.find('Dolly').transform.position += Vector3.new(0.02, 0, 0)");
    frames(*engine, 1);
  }
  CHECK(says(*engine, "Camera.trauma.zero?"));
  CHECK(says(*engine, "Camera.main.transform.local_position == $local"));
  CHECK(says(*engine, kSeesFromItsPlace));
  CHECK(engine->scripts().error_count() == 0);
}

TEST_CASE("a shake moves what the camera shows, never the camera, and fades out; shakes stack up to full trauma") {
  TempGame game(kGame);
  NullPresenter screen(kScreen);
  auto engine = game_on(game, hero_scene(), screen);
  frames(*engine, 1);
  ruby(*engine, "$place = Camera.main.transform.position; $turn = Camera.main.transform.rotation");
  CHECK(ruby(*engine, "Camera.shake(0.7, duration: 0.5)") == "0.7");
  CHECK(ruby(*engine, "Camera.shake(0.7, duration: 0.5)") == "1.0");
  frames(*engine, kFewFrames);
  CHECK(says(*engine, "!(" + std::string(kSeesFromItsPlace) + ")"));
  CHECK(says(*engine, "Camera.main.transform.position == $place && Camera.main.transform.rotation == $turn"));
  CHECK(says(*engine, "Camera.trauma.between?(0.01, 0.99)"));
  frames(*engine, kOneSecond);
  CHECK(says(*engine, "Camera.trauma.zero?"));
  CHECK(says(*engine, kSeesFromItsPlace));

  ruby(*engine, "Camera.shake(1.0)");
  frames(*engine, 1);
  ruby(*engine, "Camera.stop_shaking");
  frames(*engine, 1);
  CHECK(says(*engine, kSeesFromItsPlace));
  CHECK(fails(*engine, "Camera.shake(1.5)"));
  CHECK(fails(*engine, "Camera.shake(0.5, duration: 0)"));
}

TEST_CASE("a script turning the camera a little every frame while it shakes keeps exactly its own turns") {
  TempGame game(kGame);
  NullPresenter screen(kScreen);
  auto engine = game_on(game, hero_scene(), screen);
  frames(*engine, 1);
  ruby(*engine, "$place = Camera.main.transform.position");
  ruby(*engine, "Camera.shake(1.0, duration: 0.5)");
  for (int frame = 0; frame < kOneSecond; ++frame) {
    ruby(*engine,
         "Camera.main.transform.rotate(Vector3.new(0, 1, 0)); Camera.main.transform.translate(Vector3.right * 0.01)");
    frames(*engine, 1);
  }
  CHECK(says(*engine, "Camera.trauma.zero?"));
  CHECK(says(*engine, "Camera.main.transform.rotation.approx?(Vector3.new(0, 60, 0), 1e-3)"));  // no roll left
  CHECK(says(*engine, "Camera.main.transform.position.approx?($place + Vector3.right * 0.6, 1e-4)"));
  CHECK(says(*engine, kSeesFromItsPlace));
}

TEST_CASE("a shake on top of a follow leaves the camera where the follow puts it") {
  TempGame game(kGame);
  NullPresenter screen(kScreen);
  auto engine = game_on(game, hero_scene(), screen);
  ruby(*engine, "Camera.follow(Scene.find('Hero'), offset: Vector3.new(0, 3, 6), smoothing: 0.1)");
  frames(*engine, kOneSecond);
  ruby(*engine, "Camera.shake(1.0, duration: 1.0)");
  frames(*engine, kHalfSecond);
  CHECK(says(*engine, "Camera.main.transform.position.approx?(Vector3.new(0, 3, 6), 0.001)"));
  CHECK(says(*engine, "!(" + std::string(kSeesFromItsPlace) + ")"));
  frames(*engine, kOneSecond);
  CHECK(says(*engine, kSeesFromItsPlace));
}

TEST_CASE("the same shake under a fixed step: every run sees the camera in the same places") {
  const auto shaken_place = [] {
    TempGame game(kGame);
    NullPresenter screen(kScreen);
    auto engine = game_on(game, hero_scene(), screen);
    frames(*engine, 1);
    ruby(*engine, "Camera.shake(0.8, duration: 1.0)");
    frames(*engine, kFewFrames);
    return ruby(*engine, "Camera.screen_ray(400, 300).flat_map(&:to_a).map { |part| part.round(6) }");
  };
  CHECK(shaken_place() == shaken_place());
}

TEST_CASE("Camera.follow and Camera.orbit come in front of a wall between the camera and the target") {
  TempGame game(kGame);
  NullPresenter screen(kScreen);
  // A wall from z = 2.9 to 3.1, between the hero and where the camera wants to be.
  auto engine = game_on(game, hero_scene({node("Wall", {0.0f, 1.0f, 3.0f}, {6.0f, 6.0f, 0.2f}, solid())}), screen);
  ruby(*engine, "Camera.follow(Scene.find('Hero'), offset: Vector3.new(0, 1, 6), smoothing: 0)");
  frames(*engine, 2);
  CHECK(says(*engine, "Camera.main.transform.position.z.between?(2.5, 2.9)"));
  ruby(*engine, "Camera.follow(Scene.find('Hero'), offset: Vector3.new(0, 1, 6), smoothing: 0, avoid_walls: false)");
  frames(*engine, 1);
  CHECK(says(*engine, "Camera.main.transform.position.approx?(Vector3.new(0, 1, 6), 0.001)"));

  ruby(*engine, "Camera.orbit(Scene.find('Hero'), distance: 6, pitch: 0, smoothing: 0)");
  frames(*engine, 1);
  CHECK(says(*engine, "Camera.main.transform.position.z.between?(2.5, 2.9)"));
}

TEST_CASE("Camera.orbit turns with the mouse dragged and the stick, and zooms with the wheel, within its limits") {
  TempGame game(kGame);
  NullPresenter screen(kScreen);
  auto engine = game_on(game, hero_scene(), screen);
  ruby(*engine, "$orbit = Camera.orbit(Scene.find('Hero'), distance: 6, pitch: 20, distances: 2..10, smoothing: 0)");
  frames(*engine, 1);
  CHECK(
      says(*engine, "Camera.main.transform.position.approx?(Vector3.up + Camera::Orbit.direction(0, 20) * 6, 0.001)"));

  ScriptedInputSource& input = engine->scripted_input();
  input.move_mouse(kMouseStart, kMouseStart);
  frames(*engine, 1);
  input.move_mouse(kMouseStart + kDragged, kMouseStart);  // not dragged: the button is up
  frames(*engine, 1);
  CHECK(ruby(*engine, "$orbit.yaw.round") == "0");
  input.set_mouse_button(MouseButton::Right, true);
  input.move_mouse(kMouseStart, kMouseStart);
  frames(*engine, 1);
  CHECK(ruby(*engine, "$orbit.yaw.round") == "30");
  input.move_mouse(kMouseStart, kMouseStart + kDragged * kDragged);  // far down: held at the steepest pitch
  frames(*engine, 1);
  CHECK(ruby(*engine, "$orbit.pitch.round") == "80");
  input.set_mouse_button(MouseButton::Right, false);

  input.set_gamepad_axis(GamepadAxis::RightX, 1.0f);
  frames(*engine, kOneSecond);
  input.set_gamepad_axis(GamepadAxis::RightX, 0.0f);
  CHECK(ruby(*engine, "$orbit.yaw.round") == "-120");  // 150 degrees a second, to the right

  input.set_mouse_wheel(kWheelNotches);
  frames(*engine, 1);
  input.set_mouse_wheel(0.0f);
  CHECK(ruby(*engine, "$orbit.distance.round(2)") == "4.8");
  ruby(*engine, "$orbit.zoom(-100)");
  CHECK(ruby(*engine, "$orbit.distance") == "2.0");
  CHECK(engine->scripts().error_count() == 0);
}

TEST_CASE("Camera.world_to_screen is where the interface draws a point of the world, at any render scale") {
  TempGame game(kGame);
  NullPresenter screen(kScreen);
  auto engine = game_on(game, hero_scene(), screen);
  frames(*engine, 1);
  // The camera at the origin, looking down -Z.
  CHECK(ruby(*engine, "Camera.world_to_screen(Vector3.new(0, 0, -10))") == "[400.0, 300.0]");
  CHECK(ruby(*engine, "Camera.world_to_screen(Vector3.new(0, 0, 10))") == "");  // behind it: nil
  ruby(*engine, "Scene.find('Hero').transform.position = Vector3.new(0, 0, -5)");
  CHECK(ruby(*engine, "Camera.world_to_screen(Scene.find('Hero'))") == "[400.0, 300.0]");
  const char* round_trip =
      "ray = Camera.screen_ray(120, 470); Camera.world_to_screen(ray[0] + ray[1] * 7).zip([120, 470]).all? { |a, b| "
      "(a - b).abs < 0.01 }";
  CHECK(says(*engine, round_trip));
  ruby(*engine, "Settings.render_scale = 0.5");
  CHECK(says(*engine, round_trip));
  CHECK(ruby(*engine, "Camera.world_to_screen(Vector3.new(0, 0, -10))") == "[400.0, 300.0]");

  CHECK(fails(*engine, "Camera.world_to_screen(Vector3.new(0, 0, 0.0 / 0.0))"));
  CHECK(fails(*engine, "Camera.world_to_screen([1e300, 0, 0])"));
  CHECK(fails(*engine, "Camera.world_to_screen(nil)"));
}

TEST_CASE("Camera.world_to_screen on a screen of an odd shape, the world drawn at an odd share of it") {
  TempGame game(kGame);
  NullPresenter screen(kOddScreen);
  auto engine = game_on(game, hero_scene(), screen);
  ruby(*engine, "Settings.render_scale = 0.63");
  frames(*engine, 1);
  CHECK(says(*engine,
             "ray = Camera.screen_ray(20, 150); Camera.world_to_screen(ray[0] + ray[1] * 3).zip([20, 150]).all? { |a, "
             "b| (a - b).abs < 0.01 }"));
  CHECK(says(*engine,
             "Camera.world_to_screen(Vector3.new(0, 0, -10)).zip([166.5, 88.5]).all? { |a, b| (a - b).abs < 0.5 }"));
}

TEST_CASE("Camera.follow starts the camera in its place, unless it is told to fly there") {
  TempGame game(kGame);
  NullPresenter screen(kScreen);
  auto engine = game_on(game, hero_scene(), screen);
  ruby(*engine, "Camera.follow(Scene.find('Hero'), offset: Vector3.new(0, 3, 6), smoothing: 0.5)");
  frames(*engine, 1);
  CHECK(says(*engine, "Camera.main.transform.position.approx?(Vector3.new(0, 3, 6), 1e-4)"));
  ruby(*engine, "Camera.main.transform.position = Vector3.zero");
  ruby(*engine, "Camera.follow(Scene.find('Hero'), offset: Vector3.new(0, 3, 6), smoothing: 0.5, snap: false)");
  frames(*engine, 1);
  CHECK(says(*engine, "Camera.main.transform.position.length < 1.0"));
}

TEST_CASE("a wall right by the target keeps the camera off what it looks at, still looking at it") {
  TempGame game(kGame);
  NullPresenter screen(kScreen);
  // A wall from x = 0.2 to 0.4, between the hero's head and where the camera wants to be.
  auto engine = game_on(game, hero_scene({node("Wall", {0.3f, 1.0f, 0.0f}, {0.2f, 6.0f, 6.0f}, solid())}), screen);
  ruby(*engine, "Camera.follow(Scene.find('Hero'), offset: Vector3.new(6, 1, 0), smoothing: 0)");
  frames(*engine, 2);
  CHECK(says(*engine, "Camera.main.transform.position.distance_to(Vector3.up).between?(0.29, 0.31)"));
  CHECK(says(*engine, "Camera.main.transform.forward.x < -0.99"));  // at the hero, not snapped to -Z
  ruby(*engine, "Camera.orbit(Scene.find('Hero'), distance: 6, yaw: 90, pitch: 0, smoothing: 0)");
  frames(*engine, 1);
  CHECK(says(*engine, "Camera.main.transform.forward.x < -0.99"));
}

TEST_CASE("what the target carries is no wall: the camera is not pulled in front of it") {
  TempGame game(kGame);
  NullPresenter screen(kScreen);
  auto engine = game_on(game, shielded_scene(), screen);
  ruby(*engine, "Camera.follow(Scene.find('Hero'), offset: Vector3.new(0, 1, 6), smoothing: 0)");
  frames(*engine, 2);
  CHECK(says(*engine, "Camera.main.transform.position.approx?(Vector3.new(0, 1, 6), 0.001)"));
}

TEST_CASE("a camera that is no longer the main one stops following and shaking") {
  TempGame game(kGame);
  NullPresenter screen(kScreen);
  auto engine = game_on(game, copied_camera_scene(), screen);
  frames(*engine, 1);
  ruby(*engine, "$crate = Scene.spawn('Crate'); $first = Scene.spawn('Camera'); Scene.find('Camera').destroy");
  frames(*engine, 1);
  REQUIRE(says(*engine, "Camera.main == $first"));
  ruby(*engine, "Camera.follow(Scene.find('Hero'), offset: Vector3.new(0, 3, 6)); Camera.shake(1.0, duration: 5)");
  frames(*engine, 1);
  ruby(*engine, "$crate.destroy");
  frames(*engine, 1);
  ruby(*engine, "$second = Scene.spawn($first)");  // where the crate was: before $first
  frames(*engine, 1);
  REQUIRE(says(*engine, "Camera.main == $second"));
  ruby(*engine, "$place = $first.transform.position; Scene.find('Hero').transform.position = Vector3.new(5, 0, 0)");
  frames(*engine, kFewFrames);
  CHECK(says(*engine, "$first.transform.position == $place"));
  CHECK(says(*engine, "$first.get_component(Camera::Rig).shake.trauma.zero?"));
  CHECK(says(*engine, "!Camera.following? && Camera.trauma.zero?"));
  CHECK(engine->scripts().error_count() == 0);
}

TEST_CASE("the player turns an orbit while the game is paused, and as fast in slow motion") {
  TempGame game(kGame);
  NullPresenter screen(kScreen);
  auto engine = game_on(game, hero_scene(), screen);
  ruby(*engine, "$orbit = Camera.orbit(Scene.find('Hero'), distance: 6, pitch: 0, smoothing: 0)");
  frames(*engine, 1);
  ruby(*engine, "Time.scale = 0");
  ScriptedInputSource& input = engine->scripted_input();
  input.move_mouse(kMouseStart, kMouseStart);
  frames(*engine, 1);
  input.set_mouse_button(MouseButton::Right, true);
  input.move_mouse(kMouseStart - kDragged, kMouseStart);
  frames(*engine, 1);
  input.set_mouse_button(MouseButton::Right, false);
  CHECK(ruby(*engine, "$orbit.yaw.round") == "30");
  CHECK(
      says(*engine, "Camera.main.transform.position.approx?(Vector3.up + Camera::Orbit.direction(30, 0) * 6, 0.001)"));
  CHECK(says(*engine, "Time.delta.zero? && Time.unscaled_delta > 0"));

  ruby(*engine, "Time.scale = 0.5");
  input.set_gamepad_axis(GamepadAxis::RightX, 1.0f);
  frames(*engine, kOneSecond);
  input.set_gamepad_axis(GamepadAxis::RightX, 0.0f);
  CHECK(ruby(*engine, "$orbit.yaw.round") == "-120");  // 150 degrees in a second of the player's
}

TEST_CASE("Camera.world_to_screen follows the picture as the presenter draws it, between its bars") {
  TempGame game(kGame);
  SquarePicture screen;
  auto engine = game_on(game, hero_scene(), screen);
  frames(*engine, 1);
  // The right edge of the camera's view, ten meters away: the right edge of the square picture.
  CHECK(says(*engine,
             "Camera.world_to_screen(Vector3.new(Math.tan(22.5 * Math::PI / 180) * 10, 0, -10)).zip([600, 200]).all? "
             "{ |a, b| (a - b).abs < 0.01 }"));
  CHECK(says(*engine,
             "Camera.screen_ray(600, 200)[1].approx?(Vector3.new(Math.tan(22.5 * Math::PI / 180), 0, "
             "-1).normalized, 1e-5)"));
}

TEST_CASE("the camera helpers refuse what would break the picture") {
  TempGame game(kGame);
  NullPresenter screen(kScreen);
  auto engine = game_on(game, hero_scene(), screen);
  frames(*engine, 1);
  CHECK(fails(*engine, "Camera.follow(Scene.find('Hero'), smoothing: Float::NAN)"));
  CHECK(fails(*engine, "Camera.follow(Scene.find('Hero'), smoothing: -1)"));
  CHECK(fails(*engine, "Camera.follow(Scene.find('Hero'), offset: [0, 1e30, 0])"));
  CHECK(fails(*engine, "Camera.follow(nil)"));
  CHECK(fails(*engine, "Camera.orbit(Scene.find('Hero'), distances: 5..1)"));
  CHECK(fails(*engine, "Camera.orbit(Scene.find('Hero'), pitch: 90)"));
  CHECK(fails(*engine, "Camera.orbit(Scene.find('Hero'), distance: Float::INFINITY)"));
  CHECK(fails(*engine, "Camera.orbit(Scene.find('Hero')).turn(Float::NAN, 0)"));
  CHECK(fails(*engine, "Camera.shake(-0.1)"));
  CHECK(fails(*engine, "Camera.__shake_by(Scene.find('Hero'), Vector3.zero, Vector3.zero)"));  // not a camera
}
