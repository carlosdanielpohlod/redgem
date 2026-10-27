#pragma once

// A headless game for physics tests played through Ruby: a game folder holding only a game.json,
// a scene made in code (a camera, the ground and what a test adds), a fixed step.

#include <doctest/doctest.h>

#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <mruby.h>
#include <mruby/string.h>

#include "app/engine.hpp"
#include "core/config.hpp"
#include "core/files.hpp"

namespace redgem::test::physics_game {

namespace fs = std::filesystem;

inline constexpr int kOneSecond = 60;
inline constexpr int kTwoSeconds = kOneSecond * 2;

inline constexpr const char* kShooterGame = R"({
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

inline NodeDesc node(const std::string& name, Vec3 position, Vec3 size, PropertyMap props) {
  NodeDesc desc;
  desc.name = name;
  desc.local.position = position;
  desc.local.scale = size;  // an object without a mesh collides as a unit cube, scaled
  desc.props = std::move(props);
  return desc;
}

inline PropertyMap solid(const std::string& layer = kDefaultLayerName) {
  return {{"collider", std::string("box")}, {"layer", layer}};
}

inline PropertyMap moving(const std::string& body, const std::string& layer = kDefaultLayerName) {
  return {{"body", body}, {"layer", layer}};
}

// A camera and the ground, and whatever a test adds.
inline SceneDesc scene_with(std::vector<NodeDesc> more) {
  SceneDesc desc;
  NodeDesc camera;
  camera.name = "Camera";
  camera.camera = CameraDesc{};
  desc.nodes = {camera, node("Ground", {0, -0.5f, 0}, {80, 1, 80}, solid())};
  for (NodeDesc& each : more) desc.nodes.push_back(std::move(each));
  return desc;
}

inline std::unique_ptr<Engine> engine_for(const TempGame& game, const SceneDesc& scene) {
  EngineOptions options;
  options.game_dir = game.dir;
  options.fixed_step = true;
  options.scene = &scene;
  options.save_file = game.dir / "save.json";
  auto created = Engine::create(options);
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  return std::move(std::get<std::unique_ptr<Engine>>(created));
}

inline void frames(Engine& engine, int count) {
  for (int i = 0; i < count; ++i) engine.frame(config::kFixedDeltaSeconds);
}

inline std::string ruby(Engine& engine, const std::string& code) {
  const RubyResult result = engine.scripts().eval(code, "test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  mrb_state* mrb = engine.scripts().vm().state();
  return RSTRING_PTR(mrb_obj_as_string(mrb, result.value));
}

inline std::string error_of(Engine& engine, const std::string& code) {
  const RubyResult result = engine.scripts().eval(code, "bad.rb");
  REQUIRE_FALSE(result.ok);
  return result.error;
}

inline float number(Engine& engine, const std::string& code) { return std::stof(ruby(engine, code)); }

}  // namespace redgem::test::physics_game
