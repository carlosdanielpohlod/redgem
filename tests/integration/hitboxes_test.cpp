// Hit boxes on an animated skeleton, in a game shown by nobody (as a dedicated server runs it):
// they follow the bones' pose frame by frame, a ray meets the box where the bone is in that frame
// and says the region it hit, and what is wrong is refused.

#include <doctest/doctest.h>

#include <filesystem>
#include <memory>
#include <string>
#include <utility>

#include "integration/physics_game.hpp"

using namespace redgem;
using namespace redgem::test::physics_game;

namespace {

// The skinned fixture: a Rig holding an Arm skinned to two bones, Lower and Upper above it, whose
// one-second Wave clip bends Upper about X from upright to level (easing in and out), playing in a
// loop from the start.
// The hit box layer meets nothing: rays find it, bodies pass through it.
constexpr const char* kGame =
    R"({"title": "Hits", "physics": {"layers": ["default", "hitbox"], "collides": {"hitbox": []}}})";
constexpr int kThreeQuarterSecond = 45;  // frames: Upper is bent about 80 degrees by then
constexpr float kMoved = 1.0f;           // meters the hand box, a meter out along Upper, goes by then
constexpr float kCopyAside = 5.0f;       // meters: the copy stands well away from the first one

constexpr const char* kBuild =
    "$boxes = Hitboxes.build(Scene.find('Rig'), {"
    "  'Upper' => { region: :hand, size: 0.4, offset: [0, 1, 0] },"
    "  'Lower' => { region: :forearm, shape: :capsule, size: [0.4, 1.6, 0.4], offset: [0, 1, 0] }"
    "}, layer: 'hitbox')";

// Where a ray from in front of a point, straight at it on the hit box layer, meets: the region, or
// nil.
std::string region_at(Engine& engine, const std::string& point) {
  return ruby(engine, "p = " + point +
                          "; Hitboxes.region(Physics.raycast(p + Vector3.new(0, 0, 5), Vector3.new(0, 0, -1), 10, "
                          "layers: ['hitbox'])).inspect");
}

struct SkinnedGame {
  TempGame game{kGame};
  std::unique_ptr<Engine> engine;

  SkinnedGame() {
    std::filesystem::copy_file(test::fixture("skinned.glb"), game.dir / "scene.glb");
    EngineOptions options;
    options.game_dir = game.dir;
    options.fixed_step = true;
    options.save_file = game.dir / "save.json";
    auto created = Engine::create(options);
    if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
    engine = std::move(std::get<std::unique_ptr<Engine>>(created));
  }
};

}  // namespace

TEST_CASE("hit boxes made on a skeleton's bones carry their region and layer, under their bones") {
  SkinnedGame game;
  Engine& engine = *game.engine;
  ruby(engine, kBuild);
  CHECK(ruby(engine, "$boxes.size") == "2");
  CHECK(ruby(engine, "$boxes[0].parent.name") == "Upper");
  CHECK(ruby(engine, "$boxes[0].name") == "Upper hitbox");
  CHECK(ruby(engine, "$boxes[0].props[:region]") == "hand");
  CHECK(ruby(engine, "$boxes[1].props[:region]") == "forearm");
  CHECK(ruby(engine, "$boxes[1].rigidbody.kinematic?") == "true");
  CHECK(ruby(engine, "$boxes[0].props[:layer]") == "hitbox");
  frames(engine, 1);
  CHECK(region_at(engine, "$boxes[0].transform.position") == ":hand");
  CHECK(region_at(engine, "Scene.find('Lower').transform.position + Vector3.new(0, 0.5, 0)") == ":forearm");
  CHECK(region_at(engine, "Vector3.new(30, 0, 0)") == "nil");
}

TEST_CASE("a hit box is as large as its size says, not a meter whatever it was given") {
  SkinnedGame game;
  Engine& engine = *game.engine;
  ruby(engine, kBuild);
  frames(engine, 1);
  // The hand box is 0.4 meters a side: 0.15 from its middle is in it, 0.3 is past it.
  CHECK(region_at(engine, "$boxes[0].transform.position + Vector3.new(0.15, 0, 0)") == ":hand");
  CHECK(region_at(engine, "$boxes[0].transform.position + Vector3.new(0.3, 0, 0)") == "nil");
  CHECK(region_at(engine, "$boxes[0].transform.position + Vector3.new(-0.3, 0, 0)") == "nil");
}

TEST_CASE("the hit boxes of a copy of a character follow the copy's bones") {
  SkinnedGame game;
  Engine& engine = *game.engine;
  ruby(engine, kBuild);
  ruby(engine, "$copy = Scene.spawn(Scene.find('Rig'), position: Vector3.new(10, 0, 0))");
  frames(engine, 1);
  ruby(engine,
       "$hand = Scene.objects.find { |o| o.name == 'Upper hitbox' && o != $boxes[0] }; "
       "$before = $hand.transform.position");
  CHECK(number(engine, "$before.x") > kCopyAside);
  frames(engine, kThreeQuarterSecond);
  CHECK(number(engine, "($hand.transform.position - $before).length") > kMoved);
  CHECK(region_at(engine, "$hand.transform.position") == ":hand");  // its body went with it
  CHECK(region_at(engine, "$before") == "nil");
}

TEST_CASE("hit boxes follow the animated pose, and a ray meets the box where it is in that frame") {
  SkinnedGame game;
  Engine& engine = *game.engine;
  ruby(engine, kBuild);
  frames(engine, 1);
  ruby(engine, "$before = $boxes[0].transform.position");
  CHECK(region_at(engine, "$before") == ":hand");
  for (int frame = 0; frame < kThreeQuarterSecond; ++frame) {
    frames(engine, 1);
    CHECK(region_at(engine, "$boxes[0].transform.position") == ":hand");  // every frame, where it is now
  }
  CHECK(number(engine, "($boxes[0].transform.position - $before).length") > kMoved);
  CHECK(region_at(engine, "$before") != ":hand");  // gone from where it was
}

TEST_CASE("Hitboxes.build refuses a bone the character lacks, an unknown shape and a size that is not positive") {
  SkinnedGame game;
  Engine& engine = *game.engine;
  CHECK(error_of(engine, "Hitboxes.build(Scene.find('Rig'), { 'Tail' => {} })").find("Tail") != std::string::npos);
  CHECK(error_of(engine, "Hitboxes.build(Scene.find('Rig'), { 'Upper' => { shape: :cone } })").find("shape") !=
        std::string::npos);
  CHECK(error_of(engine, "Hitboxes.build(Scene.find('Rig'), { 'Upper' => { size: 0 } })").find("size") !=
        std::string::npos);
  CHECK(error_of(engine, "Hitboxes.build(Scene.find('Rig'), { 'Upper' => { size: [1, -1, 1] } })").find("size") !=
        std::string::npos);
  CHECK(error_of(engine, "Hitboxes.build(Scene.find('Rig'), { 'Upper' => {} }, layer: 'hitboxes')").find("hitboxes") !=
        std::string::npos);
  CHECK(ruby(engine, "Hitboxes.region(nil).inspect") == "nil");
  CHECK(ruby(engine, "Hitboxes.region({ object: Scene.find('Rig') }).inspect") == "nil");  // not a hit box
  ruby(engine, "Scene.find('Lower').props[:hitbox] = true");
  CHECK(ruby(engine, "Hitboxes.region({ object: Scene.find('Lower') }).inspect") == "nil");  // no region
}
