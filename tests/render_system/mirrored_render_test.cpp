// System test: an object whose placement mirrors it (a negative scale on one axis, as Blender's
// mirror leaves it) is drawn as the unmirrored one is, on the other side, and casts the same
// shadows, in a real window. A one-sided square shows it: drawn with the wrong winding it faces
// away and is not drawn at all.
#include <doctest/doctest.h>

#include <cmath>
#include <string>

#include "render/render_config.hpp"
#include "render_system/render_stage.hpp"
#include "scene/props.hpp"
#include "scene/shapes/primitives.hpp"

using namespace redgem;
using redgem::test::Picture;
using redgem::test::RenderStage;

namespace {

constexpr Vec3 kMirrorX{-1.0f, 1.0f, 1.0f};
constexpr Vec3 kMirrorZ{1.0f, 1.0f, -1.0f};
constexpr float kMiddle = 0.5f;  // of the picture, across and down

// The drawing test: a small glowing square lying face up, to the right of its parent, seen from above.
constexpr Vec3 kLookingDown{0.0f, 3.0f, 0.5f};
constexpr float kSquareScale = 0.05f;  // of the 10 m plane: half a meter across
constexpr float kAside = 0.6f;         // meters to the right of the parent
constexpr redgem::Color kGreen{0.1f, 1.0f, 0.1f, 1.0f};
constexpr float kGlow = 2.0f;
constexpr int kGreener = 30;        // a pixel of the square is this much greener than red or blue, at least
constexpr std::size_t kSeen = 100;  // pixels of the square, at least
constexpr float kSameSize = 0.1f;   // the mirrored square covers this share more or fewer pixels, at most
constexpr float kSameSpot = 0.05f;  // of the picture across: how far from the mirrored place it may be

// The shadow tests: a white ground, a square above it in the way of the light.
constexpr Vec3 kAboveTheGround{0.0f, 12.0f, 1.0f};
constexpr float kGroundScale = 3.0f;  // a 30 m ground: it fills the picture
constexpr Vec3 kSunCasterAt{2.0f, 1.5f, 0.0f};
constexpr float kSunCasterScale = 0.2f;  // two meters across
constexpr int kInShadow = 250;           // the sum of a pixel's channels in the sun's shadow is below this
constexpr std::size_t kSomeShadow = 100;
constexpr float kSameShadow = 0.15f;  // the mirrored caster's shadow covers this share more or fewer pixels, at most

constexpr Vec3 kLampAt{-3.0f, 4.0f, 0.0f};
constexpr Vec3 kLampCasterAt{-1.5f, 2.0f, 0.0f};
constexpr float kLampCasterScale = 0.12f;  // a little over a meter across
constexpr float kShadowWatts = 20000.0f;
constexpr float kRange = 15.0f;
constexpr float kWideCone = 0.6f;
constexpr int kVisible = 60;       // the sum of a pixel's channels where the lamp clearly reaches, at least
constexpr float kShadowed = 0.5f;  // a shadowed middle is darker than this share of a lit one

int luma(::Color c) { return c.r + c.g + c.b; }
// The square's green, paled by the sky's light around it, and not the sky's own grey-blue.
bool green(::Color c) { return c.g > c.r + kGreener && c.g > c.b + kGreener; }
bool in_sun_shadow(::Color c) { return luma(c) < kInShadow; }
int middle_of(const Picture& picture) { return luma(picture.at(kMiddle, kMiddle)); }

bool about_the_same(std::size_t a, std::size_t b, float share) {
  return std::fabs(static_cast<float>(a) - static_cast<float>(b)) <= static_cast<float>(b) * share;
}

MeshDesc mesh_of(PrimitiveShape shape, int material) {
  MeshDesc mesh{"Shape", {make_primitive(shape)}, {}};
  mesh.primitives[0].material = material;
  for (std::size_t i = 0; i < mesh.primitives[0].positions.size(); i += 3) {
    mesh.bounds.include(vec3_from(&mesh.primitives[0].positions[i]));
  }
  return mesh;
}

// An empty scaled by `parent_scale` (one, or a mirror), and under it a one-sided square of mesh
// `mesh` lying face up at `at`, `scale` times the plane's size.
void add_square(SceneDesc& desc, int mesh, Vec3 parent_scale, Vec3 at, float scale) {
  NodeDesc parent;
  parent.name = "Mirror";
  parent.local.scale = parent_scale;
  NodeDesc square;
  square.name = "Square";
  square.mesh = mesh;
  square.parent = static_cast<int>(desc.nodes.size());
  square.local.position = at;
  square.local.scale = Vec3::one() * scale;
  parent.children = {square.parent + 1};
  desc.nodes.push_back(parent);
  desc.nodes.push_back(square);
}

SceneDesc glowing_square(Vec3 parent_scale) {
  SceneDesc desc;
  desc.materials = {test::glowing_material(kGreen, kGlow)};
  desc.meshes = {mesh_of(PrimitiveShape::Plane, 0)};
  desc.nodes = {test::camera_at(kLookingDown)};
  add_square(desc, 0, parent_scale, Vec3{kAside, 0.0f, 0.0f}, kSquareScale);
  return desc;
}

NodeDesc ground() {
  NodeDesc node;
  node.name = "Ground";
  node.mesh = 0;
  node.local.scale = Vec3::one() * kGroundScale;
  return node;
}

NodeDesc environment(const PropertyMap& extra) {
  NodeDesc node;
  node.name = "Environment";
  node.props = {{props::environment::kAmbient, 0.0},
                {props::environment::kFogDensity, 0.0},
                {props::environment::kAmbientOcclusion, 0.0},
                {props::environment::kBloom, 0.0}};
  for (const auto& [key, value] : extra) node.props[key] = value;
  node.props["environment"] = true;
  return node;
}

// The default sun over a white ground, a square in its way (`parent_scale` mirrors it or not).
SceneDesc square_in_the_sun(Vec3 parent_scale) {
  SceneDesc desc;
  desc.materials = {MaterialDesc{}};
  desc.meshes = {mesh_of(PrimitiveShape::Plane, 0)};
  desc.nodes = {test::camera_at(kAboveTheGround), ground(), environment({})};
  add_square(desc, 0, parent_scale, kSunCasterAt, kSunCasterScale);
  return desc;
}

NodeDesc dark_sun() {
  NodeDesc sun;
  sun.name = "Sun";
  sun.light = LightDesc{};  // no intensity: it lights nothing (and keeps the default sun away)
  return sun;
}

NodeDesc lamp_aside(LightType type) {
  NodeDesc node;
  node.name = "Lamp";
  node.light = LightDesc{type, {1.0f, 1.0f, 1.0f, 1.0f}, kShadowWatts, kRange};
  node.light->outer_cone = kWideCone;
  node.local.position = kLampAt;
  node.local.rotation = quat_look_rotation(Vec3::zero() - kLampAt, Vec3::up());
  node.props[props::kCastShadow] = true;
  return node;
}

// A white ground lit by a point or spot lamp alone; with `blocked`, a square in its way.
SceneDesc square_under_a_lamp(LightType type, bool blocked, Vec3 parent_scale = Vec3::one()) {
  SceneDesc desc;
  desc.materials = {MaterialDesc{}};
  desc.meshes = {mesh_of(PrimitiveShape::Plane, 0)};
  desc.nodes = {test::camera_at(kAboveTheGround), ground(), dark_sun(),
                environment({{props::environment::kLocalShadows, static_cast<double>(config::kMostLocalShadows)}}),
                lamp_aside(type)};
  if (blocked) add_square(desc, 0, parent_scale, kLampCasterAt, kLampCasterScale);
  return desc;
}

Picture draw(RenderStage& stage, const SceneDesc& desc, const std::string& name) {
  stage.upload(desc);
  return stage.draw(Scene::from_desc(desc), name);
}

void check_lamp_shadow(LightType type, const std::string& name) {
  RenderStage stage;
  const Picture open = draw(stage, square_under_a_lamp(type, false), name + "_open");
  const Picture plain = draw(stage, square_under_a_lamp(type, true), name + "_plain");
  const Picture mirrored = draw(stage, square_under_a_lamp(type, true, kMirrorZ), name + "_mirrored");
  REQUIRE(middle_of(open) > kVisible);
  CHECK(static_cast<float>(middle_of(plain)) < static_cast<float>(middle_of(open)) * kShadowed);
  CHECK(static_cast<float>(middle_of(mirrored)) < static_cast<float>(middle_of(open)) * kShadowed);
}

}  // namespace

TEST_CASE("a mirrored object is drawn as the unmirrored one is, mirrored") {
  RenderStage stage;
  const Picture plain = draw(stage, glowing_square(Vec3::one()), "mirrored_square_plain");
  const Picture mirrored = draw(stage, glowing_square(kMirrorX), "mirrored_square");
  const Picture::Found right = plain.find(green);
  const Picture::Found left = mirrored.find(green);
  REQUIRE(right.count > kSeen);
  CHECK(right.across > kMiddle);
  // Drawn, not culled as a square facing away, the same size, on the other side.
  CHECK(about_the_same(left.count, right.count, kSameSize));
  CHECK(left.across < kMiddle);
  CHECK(std::fabs(left.across + right.across - 1.0f) < kSameSpot);
}

TEST_CASE("a mirrored object casts the shadow of the unmirrored one in the sun") {
  RenderStage stage;
  const Picture plain = draw(stage, square_in_the_sun(Vec3::one()), "mirrored_sun_shadow_plain");
  const Picture mirrored = draw(stage, square_in_the_sun(kMirrorZ), "mirrored_sun_shadow");
  const std::size_t plain_shadow = plain.find(in_sun_shadow).count;
  REQUIRE(plain_shadow > kSomeShadow);
  CHECK(about_the_same(mirrored.find(in_sun_shadow).count, plain_shadow, kSameShadow));
}

TEST_CASE("a mirrored object casts its shadow from spot and point lights") {
  check_lamp_shadow(LightType::Spot, "mirrored_spot_shadow");
  check_lamp_shadow(LightType::Point, "mirrored_point_shadow");
}
