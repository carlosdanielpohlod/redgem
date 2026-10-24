#include <doctest/doctest.h>

#include <filesystem>
#include <string>

#include "app/scene_files.hpp"
#include "core/config.hpp"
#include "editor/game_scenes.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::fixture;

namespace {

constexpr const char* kNewScene = "mission-1";

// Each test makes scenes in its own copy of the sandbox game.
std::filesystem::path copy_sandbox(const std::string& name) {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / ("redgem_game_scenes_" + name);
  std::filesystem::remove_all(dir);
  std::filesystem::copy(fixture("games/sandbox"), dir, std::filesystem::copy_options::recursive);
  return dir;
}

}  // namespace

TEST_CASE("a new scene's name is a safe file name scripts can load it by") {
  CHECK(valid_scene_name("level2"));
  CHECK(valid_scene_name("boss_fight"));
  CHECK(valid_scene_name(kNewScene));
  CHECK_FALSE(valid_scene_name(""));
  CHECK_FALSE(valid_scene_name("2nd"));                   // starts with a letter
  CHECK_FALSE(valid_scene_name("Level2"));                // lowercase only
  CHECK_FALSE(valid_scene_name("level 2"));               // no spaces
  CHECK_FALSE(valid_scene_name("../escape"));             // stays inside scenes/
  CHECK_FALSE(valid_scene_name("sub/level"));             // no folders
  CHECK_FALSE(valid_scene_name("level2.glb"));            // the extension is added
  CHECK_FALSE(valid_scene_name(config::kMainSceneName));  // the game's scene.glb
}

TEST_CASE("a scene file is known by the name Game.load_scene takes") {
  const std::filesystem::path game = "/games/demo";
  CHECK(scene_name_of(game, game / config::kSceneFileName) == config::kMainSceneName);
  CHECK(scene_name_of(game, game / config::kScenesDirName / "level2.glb") == "level2");
  CHECK(scene_name_of(game, scene_file(game, "level2")) == "level2");
  CHECK(scene_name_of(game, scene_file(game, config::kMainSceneName)) == config::kMainSceneName);
}

TEST_CASE("a new scene goes in scenes/, under a name no scene of the game has") {
  const std::filesystem::path game = copy_sandbox("new_file");
  std::string error;
  const auto file = new_scene_file(game, kNewScene, error);
  REQUIRE(file);
  CHECK(*file == game / config::kScenesDirName / (std::string(kNewScene) + config::kSceneExtension));
  CHECK_FALSE(new_scene_file(game, "level2", error));  // the sandbox has it
  CHECK(error.find("level2") != std::string::npos);
  error.clear();
  CHECK_FALSE(new_scene_file(game, "Bad Name", error));
  CHECK_FALSE(error.empty());
  std::filesystem::remove_all(game);
}

TEST_CASE("a new scene starts with ground, a sun and a camera looking at them") {
  const SceneDocument document = starting_scene("unused.glb");
  const SceneDesc& desc = document.desc();
  const int ground = desc.find_node("Ground");
  const int sun = desc.find_node("Sun");
  const int camera = desc.find_node("Camera");
  REQUIRE(ground != kNoIndex);
  REQUIRE(sun != kNoIndex);
  REQUIRE(camera != kNoIndex);
  CHECK(desc.nodes[ground].mesh != kNoIndex);
  REQUIRE(desc.nodes[sun].light);
  CHECK(desc.nodes[sun].light->type == LightType::Directional);
  CHECK(desc.nodes[camera].camera);
  CHECK(desc.nodes[camera].local.forward().y < 0.0f);  // looks down at the ground
}

TEST_CASE("a scene made in the editor is saved where Game.load_scene finds it, and only once") {
  const std::filesystem::path game = copy_sandbox("create");
  std::string error;
  const auto file = create_scene(game, kNewScene, error);
  REQUIRE_MESSAGE(file, error);
  CHECK(std::filesystem::exists(*file));
  const auto loaded = read_scene(game, kNewScene);  // what Game.load_scene reads
  REQUIRE(loaded);
  CHECK(loaded->find_node("Ground") != kNoIndex);
  CHECK(loaded->find_node("Camera") != kNoIndex);
  CHECK_FALSE(create_scene(game, kNewScene, error));  // it exists now
  std::filesystem::remove_all(game);
}
