// Water holding up the bodies of a running game, the camera seeing from under it, and the Water
// module scripts ask where it is.

#include <doctest/doctest.h>

#include <mruby.h>
#include <mruby/string.h>

#include <optional>
#include <string>

#include "app/engine.hpp"
#include "core/config.hpp"
#include "physics/buoyancy.hpp"
#include "scene/shapes/primitives.hpp"
#include "test_support.hpp"

using namespace redgem;

namespace {

constexpr int kSettleFrames = 900;  // fifteen seconds of fixed steps
constexpr float kCubeHalf = 0.5f;   // the cube primitive's
constexpr float kDepthTolerance = 0.06f;
constexpr Vec3 kDropFrom{0.0f, 2.0f, 0.0f};
constexpr double kLightDensity = 250.0;
constexpr double kStoneDensity = 2500.0;
constexpr float kSunkBelow = -3.0f;

MeshDesc mesh_of(PrimitiveShape shape, int material) {
  MeshDesc mesh;
  mesh.primitives = {make_primitive(shape)};
  mesh.primitives[0].material = material;
  const auto& positions = mesh.primitives[0].positions;
  constexpr std::size_t kPositionChannels = 3;
  for (std::size_t i = 0; i < positions.size(); i += kPositionChannels) mesh.bounds.include(vec3_from(&positions[i]));
  return mesh;
}

// A ten meter pond at height 0 with no collider, and a cube of `density` dropped into it.
SceneDesc pond_with(const std::string& body, std::optional<double> density) {
  SceneDesc desc;
  MaterialDesc material;
  material.water.enabled = true;
  desc.materials = {material};
  desc.meshes = {mesh_of(PrimitiveShape::Plane, 0), mesh_of(PrimitiveShape::Cube, kNoIndex)};
  NodeDesc pond;
  pond.name = "Pond";
  pond.mesh = 0;
  NodeDesc cube;
  cube.name = "Body";
  cube.mesh = 1;
  cube.local.position = kDropFrom;
  cube.props["body"] = body;
  if (density) cube.props["density"] = *density;
  desc.nodes = {pond, cube};
  return desc;
}

std::unique_ptr<Engine> engine_for(const SceneDesc& desc) {
  EngineOptions options;
  options.game_dir = test::fixture("games/sandbox");
  options.scene = &desc;
  options.fixed_step = true;
  auto created = Engine::create(options);
  if (const auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  return std::move(std::get<std::unique_ptr<Engine>>(created));
}

float settled_height(const SceneDesc& desc) {
  auto engine = engine_for(desc);
  for (int i = 0; i < kSettleFrames; ++i) engine->frame(config::kFixedDeltaSeconds);
  return engine->scene().world_transform(engine->scene().find("Body")).position.y;
}

std::string ruby(Engine& engine, const std::string& code) {
  const RubyResult result = engine.scripts().eval(code, "water_test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  return RSTRING_PTR(mrb_obj_as_string(engine.scripts().vm().state(), result.value));
}

}  // namespace

TEST_CASE("a dropped body floats at the depth its density says, a dense one sinks") {
  CHECK(settled_height(pond_with("dynamic", std::nullopt)) ==
        doctest::Approx(0.0f).epsilon(kDepthTolerance));  // half under: its middle at the surface
  const float light = settled_height(pond_with("dynamic", kLightDensity));
  const float under = floating_fraction(static_cast<float>(kLightDensity)) * kCubeHalf * kTwo;
  CHECK(light - kCubeHalf == doctest::Approx(-under).epsilon(kDepthTolerance));
  CHECK(settled_height(pond_with("dynamic", kStoneDensity)) < kSunkBelow);
}

TEST_CASE("kinematic bodies are left where scripts put them") {
  CHECK(settled_height(pond_with("kinematic", std::nullopt)) == doctest::Approx(kDropFrom.y));
}

TEST_CASE("scripts ask where the water is: its height, how deep a point is, and whether it is under") {
  auto engine = engine_for(pond_with("kinematic", std::nullopt));
  engine->frame(config::kFixedDeltaSeconds);
  CHECK(ruby(*engine, "Water.height_at(1, -2)") == "0.0");
  CHECK(ruby(*engine, "Water.height_at(20, 0).inspect") == "nil");
  CHECK(ruby(*engine, "Water.under?(Vector3.new(0, -1, 0))") == "true");
  CHECK(ruby(*engine, "Water.under?(Vector3.new(0, 1, 0))") == "false");
  CHECK(ruby(*engine, "Water.under?(Vector3.new(30, -1, 0))") == "false");
  CHECK(ruby(*engine, "Water.depth_at(Vector3.new(0, -1.5, 0))") == "1.5");
  CHECK(ruby(*engine, "Water.depth_at([30, 0, 0]).inspect") == "nil");
}
