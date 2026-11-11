// System test: point and spot lights, drawn in a real window. A spot lights only its cone and
// dims smoothly toward its edge; a point or spot light whose object says `cast_shadow` casts a
// shadow, and one that does not (or a quality without such shadows) lights through what is in
// its way.
#include <doctest/doctest.h>

#include <cstdlib>
#include <string>

#include "render/render_config.hpp"
#include "render_system/render_stage.hpp"
#include "scene/props.hpp"
#include "scene/shapes/primitives.hpp"

using namespace redgem;
using redgem::test::Picture;
using redgem::test::RenderStage;

namespace {

constexpr Vec3 kAboveTheGround{0.0f, 12.0f, 1.0f};  // the camera, looking at the middle of the ground
constexpr float kGroundScale = 3.0f;                // a 30 m ground: it fills the picture
constexpr float kMiddle = 0.5f;                     // of the picture, across and down
constexpr float kNearTheEdge = 0.8f;                // of the picture across: well outside any cone
constexpr int kVisible = 60;     // the sum of a pixel's channels where a light clearly reaches, at least
constexpr int kFaint = 30;       // ... and where it does not, at most
constexpr int kSameLight = 30;   // two pixels whose channels sum within this of each other are lit alike
constexpr float kRange = 15.0f;  // meters every light of the tests reaches

// The cone tests: a light straight above the middle of the ground.
constexpr Vec3 kStraightAbove{0.0f, 4.0f, 0.0f};
constexpr float kConeWatts = 8000.0f;
constexpr float kOuterCone = 0.5f;  // radians off the axis: a disc about 4 m across the ground
constexpr float kHardEdge = kOuterCone;
constexpr float kSoftEdge = 0.0f;          // dims from the axis all the way to the edge
constexpr float kNearlyFull = 0.8f;        // of the middle's brightness: dimmer is on the edge
constexpr float kConeShareOfPoint = 0.5f;  // a spot lights less than this share of what a point light does
constexpr float kWiderEdge = 3.0f;         // a soft edge has this many times the pixels of a hard one, at least
constexpr Vec3 kNorth{0.0f, 0.0f, -1.0f};  // the up of a light looking straight down

// The shadow tests: a lamp to the side, a block halfway between it and the middle of the ground,
// whose shadow falls on the middle (the camera sees past the block).
constexpr Vec3 kLampAt{-3.0f, 4.0f, 0.0f};
constexpr Vec3 kBlockAt{-1.5f, 2.0f, 0.0f};
constexpr float kShadowWatts = 20000.0f;
constexpr float kWideCone = 0.6f;  // radians: the spot reaches well past the block's shadow
constexpr float kShadowed = 0.5f;  // a shadowed middle is darker than this share of a lit one
constexpr double kNoLocalShadows = 0.0;
constexpr double kMostLocalShadows = config::kMostLocalShadows;

int luma(::Color c) { return c.r + c.g + c.b; }
int middle_of(const Picture& picture) { return luma(picture.at(kMiddle, kMiddle)); }
bool lit_alike(const Picture& a, const Picture& b) { return std::abs(middle_of(a) - middle_of(b)) <= kSameLight; }

MeshDesc mesh_of(PrimitiveShape shape, int material) {
  MeshDesc mesh{"Shape", {make_primitive(shape)}, {}};
  mesh.primitives[0].material = material;
  for (std::size_t i = 0; i < mesh.primitives[0].positions.size(); i += 3) {
    mesh.bounds.include(vec3_from(&mesh.primitives[0].positions[i]));
  }
  return mesh;
}

// Nothing but the light under test: no sky light, no fog, no glow, no darkened creases.
NodeDesc dark_environment(double local_shadows) {
  NodeDesc node;
  node.name = "Environment";
  node.props = {{props::environment::kAmbient, 0.0},
                {props::environment::kFogDensity, 0.0},
                {props::environment::kAmbientOcclusion, 0.0},
                {props::environment::kBloom, 0.0},
                {props::environment::kLocalShadows, local_shadows}};
  node.props["environment"] = true;
  return node;
}

NodeDesc dark_sun() {
  NodeDesc sun;
  sun.name = "Sun";
  sun.light = LightDesc{};  // no intensity: it lights nothing (and keeps the default sun away)
  return sun;
}

NodeDesc lamp(LightType type, float watts, Vec3 at, Vec3 toward, Vec3 up) {
  NodeDesc node;
  node.name = "Lamp";
  node.light = LightDesc{type, {1.0f, 1.0f, 1.0f, 1.0f}, watts, kRange};
  node.light->outer_cone = kOuterCone;
  node.local.position = at;
  node.local.rotation = quat_look_rotation(toward - at, up);
  return node;
}

NodeDesc lamp_above(LightType type, float inner_cone) {
  NodeDesc node = lamp(type, kConeWatts, kStraightAbove, Vec3::zero(), kNorth);
  node.light->inner_cone = inner_cone;
  return node;
}

NodeDesc lamp_aside(LightType type, bool cast_shadow) {
  NodeDesc node = lamp(type, kShadowWatts, kLampAt, Vec3::zero(), Vec3::up());
  node.light->outer_cone = kWideCone;
  node.props[props::kCastShadow] = cast_shadow;
  return node;
}

// A white ground filling the picture, lit by `light` alone; with `blocked`, a block in its way.
SceneDesc lit_ground(const NodeDesc& light, bool blocked, double local_shadows = kMostLocalShadows) {
  SceneDesc desc;
  desc.materials = {MaterialDesc{}};
  desc.meshes = {mesh_of(PrimitiveShape::Plane, 0), mesh_of(PrimitiveShape::Cube, 0)};
  NodeDesc ground;
  ground.name = "Ground";
  ground.mesh = 0;
  ground.local.scale = Vec3::one() * kGroundScale;
  desc.nodes = {test::camera_at(kAboveTheGround), ground, dark_sun(), dark_environment(local_shadows), light};
  if (!blocked) return desc;
  NodeDesc block;
  block.name = "Block";
  block.mesh = 1;
  block.local.position = kBlockAt;
  desc.nodes.push_back(block);
  return desc;
}

Picture draw(RenderStage& stage, const SceneDesc& desc, const std::string& name) {
  stage.upload(desc);
  return stage.draw(Scene::from_desc(desc), name);
}

std::size_t count_lit(const Picture& picture) {
  return picture.find([](::Color c) { return luma(c) > kFaint; }).count;
}

// Pixels on a light's dimming edge: lit, but well below the brightness of the middle.
std::size_t count_edge(const Picture& picture) {
  const auto full = static_cast<float>(middle_of(picture));
  return picture
      .find([full](::Color c) { return luma(c) > kFaint && static_cast<float>(luma(c)) < full * kNearlyFull; })
      .count;
}

}  // namespace

TEST_CASE("a spot light lights only its cone, while a point light lights every way") {
  RenderStage stage;
  const Picture spot = draw(stage, lit_ground(lamp_above(LightType::Spot, kSoftEdge), false), "spot_cone");
  const Picture point = draw(stage, lit_ground(lamp_above(LightType::Point, kSoftEdge), false), "spot_cone_point");
  CHECK(middle_of(spot) > kVisible);
  CHECK(luma(spot.at(kNearTheEdge, kMiddle)) < kFaint);
  CHECK(luma(point.at(kNearTheEdge, kMiddle)) > kFaint);
  CHECK(static_cast<float>(count_lit(spot)) < static_cast<float>(count_lit(point)) * kConeShareOfPoint);
}

TEST_CASE("a spot light dims smoothly from its inner cone to its outer one") {
  RenderStage stage;
  const Picture soft = draw(stage, lit_ground(lamp_above(LightType::Spot, kSoftEdge), false), "spot_cone_soft");
  const Picture hard = draw(stage, lit_ground(lamp_above(LightType::Spot, kHardEdge), false), "spot_cone_hard");
  REQUIRE(middle_of(soft) > kVisible);
  REQUIRE(middle_of(hard) > kVisible);
  CHECK(static_cast<float>(count_edge(soft)) > static_cast<float>(count_edge(hard)) * kWiderEdge);
}

TEST_CASE("a spot light with cast_shadow casts the shadow of what is in its way") {
  RenderStage stage;
  const Picture open = draw(stage, lit_ground(lamp_aside(LightType::Spot, true), false), "spot_shadow_open");
  const Picture blocked = draw(stage, lit_ground(lamp_aside(LightType::Spot, true), true), "spot_shadow");
  REQUIRE(middle_of(open) > kVisible);
  CHECK(static_cast<float>(middle_of(blocked)) < static_cast<float>(middle_of(open)) * kShadowed);
}

TEST_CASE("a point light with cast_shadow casts the shadow of what is in its way") {
  RenderStage stage;
  const Picture open = draw(stage, lit_ground(lamp_aside(LightType::Point, true), false), "point_shadow_open");
  const Picture blocked = draw(stage, lit_ground(lamp_aside(LightType::Point, true), true), "point_shadow");
  REQUIRE(middle_of(open) > kVisible);
  CHECK(static_cast<float>(middle_of(blocked)) < static_cast<float>(middle_of(open)) * kShadowed);
}

TEST_CASE("without cast_shadow, or under a quality without them, point and spot lights cast no shadow") {
  RenderStage stage;
  for (const LightType type : {LightType::Point, LightType::Spot}) {
    const std::string kind = type == LightType::Point ? "point" : "spot";
    const Picture open = draw(stage, lit_ground(lamp_aside(type, false), false), kind + "_no_shadow_open");
    const Picture through = draw(stage, lit_ground(lamp_aside(type, false), true), kind + "_no_shadow");
    const Picture held = draw(stage, lit_ground(lamp_aside(type, true), true, kNoLocalShadows), kind + "_shadows_off");
    REQUIRE(middle_of(open) > kVisible);
    CHECK(lit_alike(through, open));
    CHECK(lit_alike(held, open));
  }
}
