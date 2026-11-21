// The Environment scripting API, played through Ruby in a headless game: a script changes the
// scene's look, and what the renderer reads each frame (scene_environment) follows.

#include <doctest/doctest.h>

#include <string>

#include "physics_game.hpp"
#include "render/environment.hpp"
#include "test_support.hpp"

using namespace redgem;
using namespace redgem::test::physics_game;

namespace {

constexpr float kClose = 1e-4f;

NodeDesc environment_node() {
  NodeDesc desc;
  desc.name = "Environment";
  desc.props = {{"environment", true}, {"sky_intensity", 1.0}, {"fog_color", std::string("#ffffff")}};
  return desc;
}

}  // namespace

TEST_CASE("Environment settings changed by a script reach what the renderer reads") {
  const TempGame game(R"({"title": "Look"})");
  auto engine = engine_for(game, scene_with({environment_node()}));
  ruby(*engine, "Environment[:sky_intensity] = 0.25");
  ruby(*engine, "Environment.set(ambient: 0.5, sky_image: 'night', auto_exposure: true, fog_color: [255, 0, 0])");
  const Environment env = scene_environment(engine->scene());
  CHECK(env.sky_intensity == doctest::Approx(0.25f).epsilon(kClose));
  CHECK(env.ambient == doctest::Approx(0.5f).epsilon(kClose));
  CHECK(env.sky_image == "night");
  CHECK(env.auto_exposure.enabled);
  CHECK(env.fog_color.x > env.fog_color.y);  // red
  CHECK(ruby(*engine, "Environment[:fog_color]") == "#ff0000");
  CHECK(ruby(*engine, "Environment[:sky_intensity]") == "0.25");
}

TEST_CASE("Environment takes a color by its name and gives a setting back to its default with nil") {
  const TempGame game(R"({"title": "Look"})");
  auto engine = engine_for(game, scene_with({environment_node()}));
  ruby(*engine, "Environment[:sky_horizon] = :black");
  CHECK(ruby(*engine, "Environment[:sky_horizon]") == "#000000");
  ruby(*engine, "Environment[:sky_intensity] = nil");
  CHECK(ruby(*engine, "Environment[:sky_intensity].inspect") == "nil");
  CHECK(scene_environment(engine->scene()).sky_intensity == doctest::Approx(default_environment().sky_intensity));
}

TEST_CASE("Environment refuses a setting it does not have and a value of the wrong kind") {
  const TempGame game(R"({"title": "Look"})");
  auto engine = engine_for(game, scene_with({environment_node()}));
  CHECK(error_of(*engine, "Environment[:sky_colour] = 1").find("no setting sky_colour") != std::string::npos);
  CHECK(error_of(*engine, "Environment[:ambient] = 'bright'").find("TypeError") != std::string::npos);
  CHECK(error_of(*engine, "Environment[:fog_color] = '#zz0000'").find("#rrggbb") != std::string::npos);
  CHECK(error_of(*engine, "Environment[:auto_exposure] = 1").find("true or false") != std::string::npos);
  CHECK(error_of(*engine, "Environment[:ambient] = Float::INFINITY").find("ArgumentError") != std::string::npos);
}

TEST_CASE("Environment in a scene without an environment object reads nil and refuses changes") {
  const TempGame game(R"({"title": "Look"})");
  auto engine = engine_for(game, scene_with({}));
  CHECK(ruby(*engine, "Environment.present?") == "false");
  CHECK(ruby(*engine, "Environment[:ambient].inspect") == "nil");
  CHECK(error_of(*engine, "Environment[:ambient] = 1").find("no environment object") != std::string::npos);
}
