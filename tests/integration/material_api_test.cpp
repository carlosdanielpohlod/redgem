// The Material scripting API, played through Ruby in a headless game: what a script sets of a
// material's glow is what the renderer is handed each frame (the context's material_emission).

#include <doctest/doctest.h>

#include <string>

#include "physics_game.hpp"
#include "test_support.hpp"

using namespace redgem;
using namespace redgem::test::physics_game;

namespace {

constexpr float kClose = 1e-4f;
constexpr const char* kGame = R"({"title": "Glow"})";

}  // namespace

TEST_CASE("Material.set_emission hands the renderer the light of every material of that name") {
  const TempGame game(kGame);
  auto engine = engine_for(game, scene_with({}));
  ruby(*engine, "Material.set_emission('window_glass', [255, 0, 51], strength: 2.0)");
  const EmissionOverrides& emission = engine->script_context().material_emission;
  REQUIRE(emission.count("window_glass") == 1);
  const Vec3 light = emission.at("window_glass");
  CHECK(light.x == doctest::Approx(2.0f).epsilon(kClose));
  CHECK(light.y == doctest::Approx(0.0f).epsilon(kClose));
  CHECK(light.z == doctest::Approx(0.4f).epsilon(kClose));
  CHECK(ruby(*engine, "Material.emission(:window_glass)[:strength]") == "2.0");
}

TEST_CASE("Material.reset_emission gives the materials of that name back to the file's glow") {
  const TempGame game(kGame);
  auto engine = engine_for(game, scene_with({}));
  ruby(*engine, "Material.set_emission(:lamp, :white)");
  ruby(*engine, "Material.reset_emission(:lamp)");
  CHECK(engine->script_context().material_emission.empty());
  CHECK(ruby(*engine, "Material.emission(:lamp).inspect") == "nil");
}

TEST_CASE("Material.set_emission refuses a negative strength and an unknown color") {
  const TempGame game(kGame);
  auto engine = engine_for(game, scene_with({}));
  CHECK(error_of(*engine, "Material.set_emission(:lamp, :white, strength: -1)").find("at least") != std::string::npos);
  CHECK(error_of(*engine, "Material.set_emission(:lamp, :mauve_ish)").find("unknown color") != std::string::npos);
  CHECK(engine->script_context().material_emission.empty());
}
