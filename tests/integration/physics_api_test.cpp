// The physics scripting API, played through Ruby in a headless game with a fixed step: collision
// layers from game.json and the `layer` property, queries, forces and switching body types.

#include <doctest/doctest.h>

#include <filesystem>
#include <memory>
#include <string>

#include <mruby.h>
#include <mruby/string.h>

#include "app/engine.hpp"
#include "core/config.hpp"
#include "core/files.hpp"
#include "test_support.hpp"

using namespace redgem;
namespace fs = std::filesystem;

namespace {

constexpr int kOneSecond = 60;
constexpr int kTwoSeconds = kOneSecond * 2;

constexpr const char* kShooterGame = R"({
  "title": "Shooter",
  "physics": {
    "layers": ["default", "player", "enemy", "bullet", "pickup"],
    "collides": {"bullet": ["enemy", "default"]}
  }
})";

// A game folder of the test's own holding only a game.json, removed when the test ends.
struct TempGame {
  fs::path dir = fs::temp_directory_path() / "redgem-physics-api-test";
  explicit TempGame(const std::string& game_json) {
    fs::remove_all(dir);
    fs::create_directories(dir);
    std::string error;
    REQUIRE_MESSAGE(write_text_file(dir / "game.json", game_json, error), error);
  }
  ~TempGame() { fs::remove_all(dir); }
};

NodeDesc node(const std::string& name, Vec3 position, Vec3 size, PropertyMap props) {
  NodeDesc desc;
  desc.name = name;
  desc.local.position = position;
  desc.local.scale = size;  // an object without a mesh collides as a unit cube, scaled
  desc.props = std::move(props);
  return desc;
}

PropertyMap solid(const std::string& layer = kDefaultLayerName) {
  return {{"collider", std::string("box")}, {"layer", layer}};
}

PropertyMap moving(const std::string& body, const std::string& layer = kDefaultLayerName) {
  return {{"body", body}, {"layer", layer}};
}

// A camera and the ground, and whatever a test adds.
SceneDesc scene_with(std::vector<NodeDesc> more) {
  SceneDesc desc;
  NodeDesc camera;
  camera.name = "Camera";
  camera.camera = CameraDesc{};
  desc.nodes = {camera, node("Ground", {0, -0.5f, 0}, {80, 1, 80}, solid())};
  for (NodeDesc& each : more) desc.nodes.push_back(std::move(each));
  return desc;
}

std::unique_ptr<Engine> engine_for(const TempGame& game, const SceneDesc& scene) {
  EngineOptions options;
  options.game_dir = game.dir;
  options.fixed_step = true;
  options.scene = &scene;
  options.save_file = game.dir / "save.json";
  auto created = Engine::create(options);
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  return std::move(std::get<std::unique_ptr<Engine>>(created));
}

void frames(Engine& engine, int count) {
  for (int i = 0; i < count; ++i) engine.frame(config::kFixedDeltaSeconds);
}

std::string ruby(Engine& engine, const std::string& code) {
  const RubyResult result = engine.scripts().eval(code, "test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  mrb_state* mrb = engine.scripts().vm().state();
  return RSTRING_PTR(mrb_obj_as_string(mrb, result.value));
}

std::string error_of(Engine& engine, const std::string& code) {
  const RubyResult result = engine.scripts().eval(code, "bad.rb");
  REQUIRE_FALSE(result.ok);
  return result.error;
}

float number(Engine& engine, const std::string& code) { return std::stof(ruby(engine, code)); }

}  // namespace

TEST_CASE("a bullet on its layer flies through a pickup and stops at the wall behind it") {
  const TempGame game(kShooterGame);
  const SceneDesc scene =
      scene_with({node("Bullet", {0, 1, 0}, {0.2f, 0.2f, 0.2f}, moving("dynamic", "bullet")),
                  node("Pickup", {4, 1, 0}, {1, 1, 1}, solid("pickup")), node("Wall", {10, 1, 0}, {1, 4, 4}, solid())});
  auto engine = engine_for(game, scene);
  ruby(*engine, R"(
    $bullet = Scene.find('Bullet').rigidbody
    $bullet.gravity_scale = 0
    $bullet.velocity = Vector3.new(20, 0, 0)
  )");
  frames(*engine, kOneSecond);
  const float x = number(*engine, "Scene.find('Bullet').transform.position.x");
  CHECK(x > 4.5f);
  CHECK(x < 9.5f);
  CHECK(ruby(*engine, "$bullet.layer.inspect") == ":bullet");
}

TEST_CASE("Ruby reads and changes layers by name, and is told the names there are") {
  const TempGame game(kShooterGame);
  const SceneDesc scene = scene_with({node("Crate", {0, 1, 0}, {1, 1, 1}, moving("dynamic"))});
  auto engine = engine_for(game, scene);
  CHECK(ruby(*engine, "Physics.layers.inspect") == "[:default, :player, :enemy, :bullet, :pickup]");
  CHECK(ruby(*engine, "Scene.find('Crate').rigidbody.layer.inspect") == ":default");
  ruby(*engine, "Scene.find('Crate').rigidbody.layer = :enemy");
  CHECK(ruby(*engine, "Scene.find('Crate').rigidbody.layer.inspect") == ":enemy");
  const std::string refused = error_of(*engine, "Scene.find('Crate').rigidbody.layer = :ghost");
  CHECK(refused.find("no physics layer 'ghost'") != std::string::npos);
  CHECK(refused.find("default, player, enemy, bullet, pickup") != std::string::npos);
}

TEST_CASE("an object on a layer the game does not have goes on the default one, with a warning") {
  const TempGame game(kShooterGame);
  const SceneDesc scene = scene_with({node("Crate", {0, 1, 0}, {1, 1, 1}, moving("dynamic", "ghost"))});
  const test::LogCapture log;
  auto engine = engine_for(game, scene);
  CHECK(log.contains("object 'Crate': no physics layer 'ghost'"));
  CHECK(ruby(*engine, "Scene.find('Crate').rigidbody.layer.inspect") == ":default");
}

TEST_CASE("an overlap finds the objects inside it, on the layers asked for, but not those ignored") {
  const TempGame game(kShooterGame);
  const SceneDesc scene = scene_with({node("EnemyA", {2, 1, 0}, {1, 1, 1}, moving("kinematic", "enemy")),
                                      node("EnemyB", {-2, 1, 1}, {1, 1, 1}, moving("kinematic", "enemy")),
                                      node("Pickup", {0, 1, 2}, {1, 1, 1}, solid("pickup")),
                                      node("FarEnemy", {20, 1, 0}, {1, 1, 1}, moving("kinematic", "enemy"))});
  auto engine = engine_for(game, scene);
  ruby(*engine, "def names(hits) = hits.map { |hit| hit[:object].name }");
  CHECK(ruby(*engine, "names(Physics.overlap_sphere(Vector3.new(0, 1, 0), 3.0, layers: :enemy)).sort.inspect") ==
        R"(["EnemyA", "EnemyB"])");
  CHECK(ruby(*engine, "names(Physics.overlap_box([0, 1, 0], [6, 1, 6], layers: [:enemy, :pickup])).sort.inspect") ==
        R"(["EnemyA", "EnemyB", "Pickup"])");
  CHECK(ruby(*engine, "names(Physics.overlap_capsule([0, 1, 0], 3.0, 6.0, ignore: [Scene.find('Ground')])).size") ==
        "3");
  const std::string first = "Physics.overlap_sphere([0, 1, 0], 3.0, layers: :enemy).first";
  CHECK(number(*engine, first + "[:distance]") == doctest::Approx(1.5f).epsilon(0.05));
  CHECK(number(*engine, first + "[:normal].x") == doctest::Approx(-1.0f).epsilon(0.01));
}

TEST_CASE("Physics.raycast keeps its one ignored object, and takes layers and lists to ignore") {
  const TempGame game(kShooterGame);
  const SceneDesc scene = scene_with({node("Pickup", {0, 4, 0}, {1, 1, 1}, solid("pickup")),
                                      node("Enemy", {0, 2, 0}, {1, 1, 1}, moving("kinematic", "enemy"))});
  auto engine = engine_for(game, scene);
  ruby(*engine, "$from = Vector3.new(0, 10, 0)");
  CHECK(ruby(*engine, "Physics.raycast($from, Vector3.down)[:object].name") == "Pickup");
  CHECK(ruby(*engine, "Physics.raycast($from, Vector3.down, 20.0, Scene.find('Pickup'))[:object].name") == "Enemy");
  CHECK(ruby(*engine, "Physics.raycast($from, Vector3.down, layers: [:default])[:object].name") == "Ground");
  CHECK(ruby(*engine,
             "Physics.raycast($from, Vector3.down, ignore: [Scene.find('Pickup'), Scene.find('Enemy')])"
             "[:object].name") == "Ground");
}

TEST_CASE("raycast_all and casts answer nearest first") {
  const TempGame game(kShooterGame);
  const SceneDesc scene = scene_with({node("Pickup", {0, 4, 0}, {1, 1, 1}, solid("pickup")),
                                      node("Enemy", {0, 2, 0}, {1, 1, 1}, moving("kinematic", "enemy"))});
  auto engine = engine_for(game, scene);
  CHECK(ruby(*engine, "Physics.raycast_all([0, 10, 0], Vector3.down, 20.0).map { |h| h[:object].name }.inspect") ==
        R"(["Pickup", "Enemy", "Ground"])");
  CHECK(number(*engine, "Physics.sphere_cast([0, 10, 0], [0, 0, 0], 0.5)[:distance]") ==
        doctest::Approx(5.0f).epsilon(0.05));
  CHECK(ruby(*engine, "Physics.box_cast([0, 10, 0], [0, 0, 0], [1, 1, 1], layers: :enemy)[:object].name") == "Enemy");
  CHECK(ruby(*engine, "Physics.capsule_cast([0, 10, 0], [0, 6, 0], 0.5, 2.0).inspect") == "nil");
  CHECK(error_of(*engine, "Physics.capsule_cast([0, 10, 0], [0, 0, 0], 1.0, 1.0)").find("a capsule's height") !=
        std::string::npos);
  CHECK(error_of(*engine, "Physics.sphere_cast([0, 10, 0], [0, 0, 0], 0)").find("a sphere's radius") !=
        std::string::npos);
}

TEST_CASE("a force added every fixed_update accelerates a body") {
  const TempGame game(kShooterGame);
  PropertyMap crate = moving("dynamic");
  crate["mass"] = 2.0;
  const SceneDesc scene = scene_with({node("Crate", {0, 3, 0}, {1, 1, 1}, crate)});
  auto engine = engine_for(game, scene);
  ruby(*engine, R"(
    class Rocket < Behaviour
      def start = rigidbody.gravity_scale = 0
      def fixed_update(_dt) = rigidbody.add_force(Vector3.new(10, 0, 0))
    end
    Scene.find('Crate').add_component(Rocket)
  )");
  frames(*engine, kOneSecond + 1);  // the behaviour starts on the first frame
  CHECK(number(*engine, "Scene.find('Crate').rigidbody.velocity.x") == doctest::Approx(5.0f).epsilon(0.05));
  CHECK(number(*engine, "Scene.find('Crate').rigidbody.mass") == doctest::Approx(2.0f));
  ruby(*engine, "Scene.find('Crate').rigidbody.angular_velocity = Vector3.new(0, 90, 0)");
  CHECK(number(*engine, "Scene.find('Crate').rigidbody.angular_velocity.y") == doctest::Approx(90.0f).epsilon(0.01));
}

TEST_CASE("a body switched to kinematic is moved by scripts, and falls once dynamic again") {
  const TempGame game(kShooterGame);
  const SceneDesc scene = scene_with({node("Crate", {0, 3, 0}, {1, 1, 1}, moving("dynamic"))});
  auto engine = engine_for(game, scene);
  ruby(*engine, "$crate = Scene.find('Crate'); $crate.rigidbody.kinematic = true");
  CHECK(ruby(*engine, "$crate.rigidbody.kinematic?") == "true");
  frames(*engine, kOneSecond);
  CHECK(number(*engine, "$crate.transform.position.y") == doctest::Approx(3.0f).epsilon(0.01));
  ruby(*engine, "$crate.transform.position = Vector3.new(2, 5, 0)");
  frames(*engine, 1);
  CHECK(number(*engine, "$crate.transform.position.y") == doctest::Approx(5.0f).epsilon(0.01));
  ruby(*engine, "$crate.rigidbody.dynamic = true");
  CHECK(ruby(*engine, "$crate.rigidbody.dynamic?") == "true");
  frames(*engine, kTwoSeconds);
  CHECK(number(*engine, "$crate.transform.position.y") == doctest::Approx(0.5f).epsilon(0.05));
}

TEST_CASE("a sleeping body hangs in the air until a script wakes it") {
  const TempGame game(kShooterGame);
  const SceneDesc scene = scene_with({node("Crate", {0, 3, 0}, {1, 1, 1}, moving("dynamic"))});
  auto engine = engine_for(game, scene);
  ruby(*engine, "$body = Scene.find('Crate').rigidbody; $body.sleep");
  frames(*engine, kOneSecond);
  CHECK(ruby(*engine, "$body.sleeping?") == "true");
  CHECK(number(*engine, "Scene.find('Crate').transform.position.y") == doctest::Approx(3.0f).epsilon(0.01));
  ruby(*engine, "$body.wake");
  frames(*engine, kTwoSeconds);
  CHECK(number(*engine, "Scene.find('Crate').transform.position.y") == doctest::Approx(0.5f).epsilon(0.05));
}
