// Save.checkpoint and Save.restore_checkpoint: spawned objects whose behaviours `persist` fields
// come back after a restart, with their template, pose and fields; others are not kept.

#include <doctest/doctest.h>
#include <string>
#include "physics_game.hpp"
#include "scene/scene_ids.hpp"

using namespace redgem;
using namespace redgem::test::physics_game;
namespace {
// A behaviour that keeps two of its fields (and not a third), and one that keeps none.
constexpr const char* kClasses = R"ruby(
  class Loot < Behaviour
    persist :health, :items
    attr_accessor :health, :items, :mood
    def start = ($started_with = @health)
  end
  class Plain < Behaviour
    attr_accessor :health
  end
)ruby";
constexpr const char* kCopies = "Scene.objects.count { |object| object.name == 'Crate' && object.stable_id.nil? }";

SceneDesc crate_scene() {
  auto desc = scene_with({node("Crate", {0, 1, 0}, {1, 1, 1}, {{"prefab", true}})});
  assign_scene_ids(desc);
  return desc;
}
void spawn_loot(Engine& engine, const char* where, const char* fields) {
  ruby(engine, std::string("$copy = Scene.spawn(Scene.find('Crate'), position: ") + where + ")");
  ruby(engine, std::string("$loot = $copy.add_component(Loot); ") + fields);
}
}  // namespace

TEST_CASE("spawned objects with persisted fields come back after a restart") {
  const TempGame game(kShooterGame);
  const auto desc = crate_scene();
  {
    auto engine = engine_for(game, desc);
    ruby(*engine, kClasses);
    spawn_loot(*engine, "[3, 1, 5]", "$loot.health = 7; $loot.items = ['gem', 'key']; $loot.mood = 'sad'");
    ruby(*engine, "$copy.tint = [0.2, 0.3, 0.4]");
    CHECK(ruby(*engine, "Save.checkpoint") == "1");
  }
  auto engine = engine_for(game, desc);
  ruby(*engine, kClasses);
  CHECK(ruby(*engine, "$made = Save.restore_checkpoint; $made.size") == "1");
  CHECK(number(*engine, "$made.first.transform.position.x") == doctest::Approx(3));
  CHECK(number(*engine, "$made.first.transform.position.z") == doctest::Approx(5));
  CHECK(number(*engine, "$made.first.tint.y") == doctest::Approx(0.3));
  CHECK(ruby(*engine, "$made.first.behaviours.size") == "1");
  CHECK(ruby(*engine, "$back = $made.first.get_component(Loot); $back.health") == "7");
  CHECK(ruby(*engine, "$back.items") == R"(["gem", "key"])");
  CHECK(ruby(*engine, "$back.mood.nil?") == "true");
  frames(*engine, 1);
  CHECK(ruby(*engine, "$started_with") == "7");
}

TEST_CASE("spawned objects that do not opt in are not saved") {
  const TempGame game(kShooterGame);
  const auto desc = crate_scene();
  {
    auto engine = engine_for(game, desc);
    ruby(*engine, kClasses);
    ruby(*engine, "Scene.spawn(Scene.find('Crate')).add_component(Plain).health = 3");
    ruby(*engine, "Scene.spawn(Scene.find('Crate'))");
    CHECK(ruby(*engine, "Save.checkpoint") == "0");
  }
  auto engine = engine_for(game, desc);
  ruby(*engine, kClasses);
  CHECK(ruby(*engine, "Save.restore_checkpoint") == "[]");
  CHECK(ruby(*engine, kCopies) == "0");
}

TEST_CASE("destroyed spawned objects are not resurrected, and a restore replaces rather than adds") {
  const TempGame game(kShooterGame);
  const auto desc = crate_scene();
  {
    auto engine = engine_for(game, desc);
    ruby(*engine, kClasses);
    spawn_loot(*engine, "[1, 1, 0]", "$loot.health = 1; $first = $copy");
    spawn_loot(*engine, "[2, 1, 0]", "$loot.health = 2");
    ruby(*engine, "$first.destroy");
    frames(*engine, 1);
    CHECK(ruby(*engine, "Save.checkpoint") == "1");
  }
  auto engine = engine_for(game, desc);
  ruby(*engine, kClasses);
  CHECK(ruby(*engine, "Save.restore_checkpoint.first.get_component(Loot).health") == "2");
  CHECK(ruby(*engine, "Save.restore_checkpoint.size") == "1");
  frames(*engine, 1);
  CHECK(ruby(*engine, kCopies) == "1");
}

TEST_CASE("a checkpoint of another scene or naming an unknown behaviour changes nothing") {
  const TempGame game(kShooterGame);
  auto engine = engine_for(game, crate_scene());
  ruby(*engine, kClasses);
  spawn_loot(*engine, "[1, 1, 0]", "$loot.health = 1");
  ruby(*engine, "Save.checkpoint(:here); Save[:elsewhere] = Save[:here].merge('scene' => 'another')");
  ruby(*engine, "$data = Save[:here]; $data['objects'][0]['behaviours'][0]['class'] = 'NoSuchLoot'");
  ruby(*engine, "Save[:unknown] = $data");
  CHECK(ruby(*engine, "begin; Save.restore_checkpoint(:elsewhere); rescue ArgumentError; 'no'; end") == "no");
  CHECK(ruby(*engine, "begin; Save.restore_checkpoint(:unknown); rescue ArgumentError; 'no'; end") == "no");
  frames(*engine, 1);
  CHECK(ruby(*engine, kCopies) == "1");
}
