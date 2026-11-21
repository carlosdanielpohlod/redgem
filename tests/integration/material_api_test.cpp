// The Material scripting API, played through Ruby in a headless game: what a script sets of a
// material's glow is what the renderer is handed each frame (the context's material_emission).

#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "physics_game.hpp"
#include "scene/color_space.hpp"
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
  CHECK(light.z == doctest::Approx(linear_from_srgb(51.0f / 255.0f) * 2.0f).epsilon(kClose));  // linear light
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

TEST_CASE("Material.set_emission refuses a strength, a color or a channel it cannot glow with") {
  const TempGame game(kGame);
  auto engine = engine_for(game, scene_with({}));
  const std::vector<std::string> refused = {
      "Material.set_emission(:lamp, :white, strength: -1)", "Material.set_emission(:lamp, :black, strength: -5)",
      "Material.set_emission(:lamp, :white, strength: 1e40)", "Material.set_emission(:lamp, [1000, 0, 0])",
      "Material.set_emission(:lamp, :mauve_ish)"};
  for (const std::string& line : refused) {
    CAPTURE(line);
    CHECK(error_of(*engine, line).find("Error") != std::string::npos);
  }
  CHECK(engine->script_context().material_emission.empty());
  CHECK(ruby(*engine, "Material.emission(:lamp).inspect") == "nil");
}

TEST_CASE("Material.emission keeps the color it was given, whatever the script does to it later") {
  const TempGame game(kGame);
  auto engine = engine_for(game, scene_with({}));
  CHECK(ruby(*engine, "c = [255, 0, 0]; Material.set_emission(:x, c); c[0] = 0; Material.emission(:x)[:color]") ==
        "[255, 0, 0]");
}
