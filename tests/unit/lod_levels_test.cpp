// Levels of detail: the import settings that ask for them, the lighter meshes made on import, the
// props each level gets, and the distances the renderer draws each level at.

#include <doctest/doctest.h>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "core/import_settings.hpp"
#include "editor/document/scene_document.hpp"
#include "editor/lod_levels.hpp"
#include "render/screen_levels.hpp"
#include "scene/props.hpp"
#include "scene/scene.hpp"
#include "scene/shapes/primitives.hpp"
#include "temp_dirs.hpp"

using namespace redgem;
using namespace redgem::lod_levels;
using import_settings::Kind;
using import_settings::ModelSettings;

namespace {

constexpr float kTolerance = 1e-4f;
constexpr float kFirstSize = 0.4f;
constexpr float kHalfRatio = 0.5f;
constexpr float kTanHalfFov = 0.5f;
constexpr float kRadius = 2.0f;
constexpr float kFade = 0.1f;
constexpr float kNear = 1.0f;
constexpr float kFar = 100.0f;
constexpr int kTwoLevels = 2;
constexpr std::size_t kBallParts = 1;
constexpr const char* kBall = "Ball";

ModelSettings with_levels(int levels) {
  ModelSettings settings;
  settings.lod_levels = levels;
  settings.lod_ratio = kHalfRatio;
  settings.lod_screen_size = kFirstSize;
  settings.lod_fade = kFade;
  return settings;
}

SceneDocument ball_scene(int& ball) {
  SceneDocument document = SceneDocument::create_empty(redgem::test::temp_root() / "redgem_lod_ball.glb");
  ball = document.add_primitive(PrimitiveShape::Sphere, kBall, redgem::Color{}, kNoIndex);
  return document;
}

const MeshDesc& mesh_of(const SceneDocument& document, int node) {
  return document.desc().meshes.at(static_cast<std::size_t>(document.node(node).mesh));
}

float number(const NodeDesc& node, const char* key) { return static_cast<float>(*get_number(node.props, key)); }

GameObject object_with(ScreenLevel level) {
  GameObject object;
  object.screen_level = level;
  return object;
}

}  // namespace

TEST_CASE("a model's import settings ask for levels of detail, within their ranges") {
  std::string error;
  const auto read = import_settings::parse(
      Kind::Model, R"({"lod_levels": 3, "lod_ratio": 0.25, "lod_screen_size": 0.3, "lod_fade": 0.2})", error);
  REQUIRE_MESSAGE(read.has_value(), error);
  const auto& model = std::get<ModelSettings>(*read);
  CHECK(model.lod_levels == 3);
  CHECK(model.lod_ratio == doctest::Approx(0.25f));
  CHECK(model.lod_screen_size == doctest::Approx(0.3f));
  CHECK(model.lod_fade == doctest::Approx(0.2f));
  CHECK(std::get<ModelSettings>(*import_settings::parse(Kind::Model, import_settings::text_of(model), error)) == model);
  CHECK_FALSE(import_settings::parse(Kind::Model, R"({"lod_levels": 5})", error).has_value());
  CHECK_FALSE(import_settings::parse(Kind::Model, R"({"lod_levels": 1.5})", error).has_value());
  CHECK_FALSE(import_settings::parse(Kind::Model, R"({"lod_ratio": 1})", error).has_value());
  CHECK_FALSE(import_settings::parse(Kind::Model, R"({"lod_screen_size": 0})", error).has_value());
  CHECK(ModelSettings{}.lod_levels == import_settings::kNoLods);
}

TEST_CASE("each level of detail is drawn below the size on screen of the one before, halving") {
  const ScreenRange whole = screen_range(0, kTwoLevels, kFirstSize);
  CHECK(whole.from == doctest::Approx(kFirstSize));
  CHECK(std::isinf(whole.to));
  const ScreenRange first = screen_range(1, kTwoLevels, kFirstSize);
  CHECK(first.to == doctest::Approx(kFirstSize));
  CHECK(first.from == doctest::Approx(kFirstSize * kHalfRatio));
  const ScreenRange last = screen_range(kTwoLevels, kTwoLevels, kFirstSize);
  CHECK(last.to == doctest::Approx(kFirstSize * kHalfRatio));
  CHECK(last.from == 0.0f);
}

TEST_CASE("a mesh is simplified to about the share of its triangles asked for, on the same vertices") {
  const PrimitiveDesc ball = make_primitive(PrimitiveShape::Sphere);
  const std::vector<std::uint32_t> half = simplified(ball, ball.indices, kHalfRatio);
  REQUIRE_FALSE(half.empty());
  CHECK(half.size() % 3 == 0);
  CHECK(half.size() < ball.indices.size());
  CHECK(static_cast<float>(half.size()) <= static_cast<float>(ball.indices.size()) * 0.75f);
  const std::size_t vertices = ball.positions.size() / 3;
  for (std::uint32_t index : half) CHECK(index < vertices);
}

TEST_CASE("vertices where a part meets another stay put when it is simplified") {
  const PrimitiveDesc ball = make_primitive(PrimitiveShape::Sphere);
  const VertexLocks all(ball.positions.size() / 3, 1);
  CHECK(simplified(ball, ball.indices, kHalfRatio, all).size() == ball.indices.size());
  CHECK(simplified(ball, ball.indices, kHalfRatio, {}).size() < ball.indices.size());
}

TEST_CASE("only a mesh no skeleton or shape key moves is simplified") {
  MeshDesc mesh;
  mesh.primitives.push_back(make_primitive(PrimitiveShape::Sphere));
  CHECK(simplifiable(mesh));
  CHECK(triangles(mesh) == mesh.primitives[0].indices.size() / 3);
  MeshDesc skinned = mesh;
  skinned.primitives[0].joints.assign(mesh.primitives[0].positions.size() / 3 * 4, 0);
  CHECK_FALSE(simplifiable(skinned));
  MeshDesc shaped = mesh;
  shaped.primitives[0].targets.emplace_back();
  CHECK_FALSE(simplifiable(shaped));
}

TEST_CASE("an imported model's mesh gets lighter levels as its children, each with when it is drawn") {
  int ball = kNoIndex;
  SceneDocument document = ball_scene(ball);
  const std::size_t full = triangles(mesh_of(document, ball));
  const std::vector<LevelCounts> counts = make_levels(document, ball, with_levels(kTwoLevels));
  REQUIRE(counts.size() == 1);
  REQUIRE(counts[0].triangles.size() == kTwoLevels + 1);
  CHECK(counts[0].triangles[0] == full);
  CHECK(counts[0].triangles[1] < full);
  CHECK(counts[0].triangles[2] < counts[0].triangles[1]);
  const NodeDesc& owner = document.node(ball);
  REQUIRE(owner.children.size() == kTwoLevels);
  CHECK(number(owner, props::kScreenSizeFrom) == doctest::Approx(kFirstSize));
  CHECK_FALSE(get_number(owner.props, props::kScreenSizeTo).has_value());
  CHECK(number(owner, props::kLodFade) == doctest::Approx(kFade));
  const NodeDesc& last = document.node(owner.children.back());
  CHECK(last.name == std::string(kBall) + " LOD 2");
  CHECK_FALSE(get_number(last.props, props::kScreenSizeFrom).has_value());
  CHECK(number(last, props::kScreenSizeTo) == doctest::Approx(kFirstSize * kHalfRatio));
  CHECK(number(last, props::kLodRadius) == doctest::Approx(number(owner, props::kLodRadius)));
  CHECK(mesh_of(document, owner.children.back()).primitives.size() == kBallParts);
  CHECK(triangles(mesh_of(document, owner.children.back())) == counts[0].triangles[2]);
  CHECK(counts_text(counts) == std::string(kBall) + ": " + std::to_string(full) + ", " +
                                   std::to_string(counts[0].triangles[1]) + ", " +
                                   std::to_string(counts[0].triangles[2]) + " triangles\n");
}

TEST_CASE("the Inspector counts a mesh node's triangles at each of its levels") {
  int ball = kNoIndex;
  SceneDocument document = ball_scene(ball);
  const std::size_t full = triangles(mesh_of(document, ball));
  CHECK(level_triangles(document.desc(), ball) == std::vector<std::size_t>{full});
  const std::vector<LevelCounts> counts = make_levels(document, ball, with_levels(kTwoLevels));
  CHECK(level_triangles(document.desc(), ball) == counts[0].triangles);
  CHECK(counts_list({3, 2, 1}) == "3, 2, 1");
  const int empty = document.add_empty("Nothing", kNoIndex);
  CHECK(level_triangles(document.desc(), empty).empty());
}

TEST_CASE("a level looks like its mesh's object, and a mesh object under it is no level of it") {
  int ball = kNoIndex;
  SceneDocument document = ball_scene(ball);
  document.set_prop(ball, props::kTint, std::string("#ff0000"), false);
  document.set_prop(ball, props::kCastShadow, false, false);
  const int wheel = document.add_primitive(PrimitiveShape::Sphere, "Wheel", redgem::Color{}, ball);
  const std::vector<LevelCounts> counts = make_levels(document, ball, with_levels(kTwoLevels));
  REQUIRE(counts.size() == 2);  // the ball and the wheel
  const std::vector<int>& children = document.node(ball).children;
  REQUIRE(children.size() == kTwoLevels + 1);
  const NodeDesc& first = document.node(children[1]);  // (after the wheel)
  CHECK(first.name == std::string(kBall) + " LOD 1");
  CHECK(get_string(first.props, props::kTint) == "#ff0000");
  CHECK(get_bool(first.props, props::kCastShadow) == false);
  CHECK(number(first, props::kLodLevel) == doctest::Approx(1.0f));
  CHECK_FALSE(document.node(ball).props.count(props::kLodLevel));
  CHECK(level_triangles(document.desc(), ball) == counts[0].triangles);
  CHECK(level_triangles(document.desc(), wheel) == counts[1].triangles);
}

TEST_CASE("no level of detail is made when none is asked for") {
  int ball = kNoIndex;
  SceneDocument document = ball_scene(ball);
  CHECK(make_levels(document, ball, with_levels(import_settings::kNoLods)).empty());
  CHECK(document.node(ball).children.empty());
  CHECK(document.node(ball).props.count(props::kLodRadius) == 0);
}

TEST_CASE("a level's props become how big on screen its object is drawn") {
  const SceneDesc desc = [] {
    SceneDesc scene;
    NodeDesc node;
    node.name = "Level";
    node.props = {
        {props::kScreenSizeFrom, 0.1}, {props::kScreenSizeTo, 0.4}, {props::kLodRadius, 2.0}, {props::kLodFade, 0.2}};
    scene.nodes.push_back(node);
    return scene;
  }();
  const Scene scene = Scene::from_desc(desc);
  const ScreenLevel& level = scene.at(0).screen_level;
  CHECK(level.from == doctest::Approx(0.1f));
  CHECK(level.to == doctest::Approx(0.4f));
  CHECK(level.radius == doctest::Approx(2.0f));
  CHECK(level.fade == doctest::Approx(0.2f));
}

TEST_CASE("sizes on screen become distances, and two levels next to each other cross over") {
  // A sphere of radius r covers s of the view's height at r / (s * tan(fov / 2)).
  const GameObject near_level = object_with({kFirstSize, std::numeric_limits<float>::infinity(), kRadius, kFade});
  const GameObject far_level = object_with({0.0f, kFirstSize, kRadius, kFade});
  const render_math::DrawnRange near_range = render_math::drawn_range(near_level, 1.0f, kTanHalfFov);
  const render_math::DrawnRange far_range = render_math::drawn_range(far_level, 1.0f, kTanHalfFov);
  const float switch_at = kRadius / (kFirstSize * kTanHalfFov);
  const float fade = kFade * kRadius / kTanHalfFov;
  CHECK(near_range.from == 0.0f);
  CHECK(near_range.to == doctest::Approx(switch_at));
  CHECK(near_range.fade == doctest::Approx(fade));
  CHECK(far_range.from == doctest::Approx(switch_at - fade));
  CHECK(std::isinf(far_range.to));
  // Twice as big in the world, it switches twice as far.
  CHECK(render_math::drawn_range(near_level, 2.0f, kTanHalfFov).to == doctest::Approx(switch_at * 2.0f));
  CHECK(render_math::longest_axis(Vec3{1.0f, -3.0f, 2.0f}) == doctest::Approx(3.0f));
}

TEST_CASE("an object without a level of detail keeps its own distances and does not fade") {
  GameObject object;
  object.visible_from = kNear;
  object.visible_to = kFar;
  const render_math::DrawnRange range = render_math::drawn_range(object, 1.0f, kTanHalfFov);
  CHECK(range.from == doctest::Approx(kNear));
  CHECK(range.to == doctest::Approx(kFar));
  CHECK(range.fade == 0.0f);
  object.screen_level = {kFirstSize, std::numeric_limits<float>::infinity(), kRadius, kFade};
  object.visible_to = kTolerance;  // its own range still holds, the nearer of the two
  CHECK(render_math::drawn_range(object, 1.0f, kTanHalfFov).to == doctest::Approx(kTolerance));
}
