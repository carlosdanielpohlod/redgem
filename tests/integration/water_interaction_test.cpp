#include <doctest/doctest.h>

#include "app/engine.hpp"
#include "core/config.hpp"
#include "scene/shapes/primitives.hpp"
#include "test_support.hpp"

using namespace redgem;

namespace {

constexpr float kImpactVelocity = -40.0f;  // crosses the water in a few fixed steps
constexpr int kImpactFrames = 8;
constexpr int kPausedFrames = 5;
constexpr Vec3 kBodyStart{0.0f, 2.0f, 0.0f};
constexpr Vec3 kBodyExit{0.0f, -2.0f, 0.0f};
constexpr Vec3 kOutsideWater{20.0f, 2.0f, 0.0f};

class WaterPresenter final : public Presenter {
 public:
  SurfaceSize surface_size() const override { return {640, 360}; }
  int text_width(const std::string&, int) const override { return 0; }
  void capture_cursor(bool) override {}
  bool open() const override { return true; }
  float frame_seconds() const override { return config::kFixedDeltaSeconds; }
  void present(const FrameView& view) override {
    REQUIRE(view.water_ripples != nullptr);
    snapshot = *view.water_ripples;
  }
  WaterRippleSnapshot snapshot;
};

MeshDesc interaction_mesh(PrimitiveShape shape, int material) {
  MeshDesc mesh;
  mesh.primitives = {make_primitive(shape)};
  mesh.primitives[0].material = material;
  const auto& positions = mesh.primitives[0].positions;
  constexpr std::size_t kPositionChannels = 3;
  for (std::size_t i = 0; i < positions.size(); i += kPositionChannels) mesh.bounds.include(vec3_from(&positions[i]));
  return mesh;
}

SceneDesc runtime_pond(bool water = true, Vec3 body_position = kBodyStart) {
  SceneDesc desc;
  MaterialDesc material;
  material.water.enabled = water;
  desc.materials = {material};
  desc.meshes = {interaction_mesh(PrimitiveShape::Plane, 0), interaction_mesh(PrimitiveShape::Cube, kNoIndex)};
  NodeDesc pond;
  pond.name = "Pond";
  pond.mesh = 0;  // deliberately no collider: waves must follow the rendered water, not contact callbacks
  NodeDesc body;
  body.name = "Body";
  body.mesh = 1;
  body.local.position = body_position;
  body.props["body"] = std::string("dynamic");
  desc.nodes = {pond, body};
  return desc;
}

std::unique_ptr<Engine> water_engine(const SceneDesc& desc, WaterPresenter& presenter) {
  EngineOptions options;
  options.game_dir = test::fixture("games/sandbox");
  options.scene = &desc;
  options.presenter = &presenter;
  options.fixed_step = true;
  auto created = Engine::create(options);
  if (const auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  return std::move(std::get<std::unique_ptr<Engine>>(created));
}

void impact_frames(Engine& engine) {
  engine.physics().set_velocity(engine.scene().find("Body"), {0.0f, kImpactVelocity, 0.0f});
  for (int i = 0; i < kImpactFrames; ++i) engine.frame(config::kFixedDeltaSeconds);
}

}  // namespace

TEST_CASE("physics automatically creates water waves and hands simulation snapshots to the presenter") {
  WaterPresenter presenter;
  const SceneDesc desc = runtime_pond();
  auto engine = water_engine(desc, presenter);
  impact_frames(*engine);
  REQUIRE(presenter.snapshot.count == 1);
  CHECK(presenter.snapshot.ripples[0].surface == engine->scene().handle(0));
  CHECK(presenter.snapshot.ripples[0].primitive == 0);
  CHECK(presenter.snapshot.ripples[0].age > 0.0f);
  const auto paused = presenter.snapshot;
  engine->script_context().time.scale = 0.0f;
  for (int i = 0; i < kPausedFrames; ++i) engine->frame(config::kFixedDeltaSeconds);
  CHECK(presenter.snapshot.time == paused.time);
  CHECK(presenter.snapshot.ripples[0].age == paused.ripples[0].age);
  engine->present();
  CHECK(presenter.snapshot.time == paused.time);
}

TEST_CASE("physics bodies outside water or crossing a dry material emit no water waves") {
  for (const bool wet_material : {false, true}) {
    WaterPresenter presenter;
    const SceneDesc desc = runtime_pond(wet_material, wet_material ? kOutsideWater : kBodyStart);
    auto engine = water_engine(desc, presenter);
    impact_frames(*engine);
    CHECK(presenter.snapshot.count == 0);
  }
}

TEST_CASE("water triggers and script teleports do not masquerade as physical impacts") {
  WaterPresenter presenter;
  SceneDesc desc = runtime_pond();
  desc.nodes[1].props["trigger"] = true;
  auto trigger_engine = water_engine(desc, presenter);
  trigger_engine->frame(config::kFixedDeltaSeconds);
  trigger_engine->scene().set_world_position(1, kBodyExit);
  trigger_engine->frame(config::kFixedDeltaSeconds);
  CHECK(presenter.snapshot.count == 0);
  trigger_engine.reset();
  desc = runtime_pond();
  auto engine = water_engine(desc, presenter);
  engine->frame(config::kFixedDeltaSeconds);
  const auto result =
      engine->scripts().eval("Scene.find('Body').transform.position = Vector3.new(0, -2, 0)", "water_test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  engine->frame(config::kFixedDeltaSeconds);
  CHECK(presenter.snapshot.count == 0);
}

TEST_CASE("scripted kinematic bodies entering water generate waves instead of being treated as teleports") {
  WaterPresenter presenter;
  SceneDesc desc = runtime_pond();
  desc.nodes[1].props["body"] = std::string("kinematic");
  auto engine = water_engine(desc, presenter);
  const auto result =
      engine->scripts().eval("Scene.find('Body').transform.position = Vector3.new(0, -2, 0)", "water_test.rb");
  REQUIRE_MESSAGE(result.ok, result.error);
  engine->frame(config::kFixedDeltaSeconds);
  REQUIRE(presenter.snapshot.count == 1);
  CHECK(presenter.snapshot.ripples[0].surface == engine->scene().handle(0));
}
