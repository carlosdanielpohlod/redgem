// GameObject#morph and #morph_names, and clips that move shape keys, in a running game: the square
// of tests/fixtures/morph.glb (shape keys Smile and Wide) under an empty "Head".
#include <doctest/doctest.h>

#include "physics_game.hpp"
#include "scene/gltf/gltf_loader.hpp"
#include "scene/scene_ids.hpp"
#include "test_support.hpp"

using namespace redgem;
using namespace redgem::test::physics_game;

namespace {

constexpr int kHead = 2;  // after the camera and the ground of scene_with
constexpr int kFace = 3;
constexpr std::size_t kSmile = 0;
constexpr std::size_t kWide = 1;
constexpr float kHalfway = 0.5f;  // seconds into the one-second clip Talk

SceneDesc face_under_a_head() {
  const LoadResult loaded = load_gltf_scene(redgem::test::fixture("morph.glb"));
  REQUIRE_MESSAGE(loaded.ok, loaded.error);
  NodeDesc head;
  head.name = "Head";
  NodeDesc face;
  face.name = "Face";
  face.mesh = 0;
  SceneDesc desc = scene_with({head, face});
  desc.nodes[kHead].children = {kFace};
  desc.nodes[kFace].parent = kHead;
  desc.meshes = loaded.scene.meshes;
  desc.animations = loaded.scene.animations;
  for (AnimationChannel& channel : desc.animations.front().channels) channel.node = kFace;
  assign_scene_ids(desc);
  return desc;
}

const GameObject& face(Engine& engine) { return engine.scene().at(kFace); }

}  // namespace

TEST_CASE("a model's shape keys are named, read and set from scripts through it and its parts") {
  const TempGame game(kShooterGame);
  const SceneDesc desc = face_under_a_head();
  auto engine = engine_for(game, desc);
  ruby(*engine, "Animation.stop(:Talk)");  // nothing but the scripts moves them
  CHECK(ruby(*engine, "Scene.find('Head').morph_names.inspect") == "[:Smile, :Wide]");
  CHECK(number(*engine, "Scene.find('Face').morph(:Smile)") == doctest::Approx(0.25));  // the mesh's own weight
  CHECK(number(*engine, "Scene.find('Head').morph('Wide', 0.7)") == doctest::Approx(0.7));
  CHECK(face(*engine).morph_weights[kWide] == doctest::Approx(0.7f));
  CHECK(number(*engine, "Scene.find('Head').morph(:Wide)") == doctest::Approx(0.7));
  CHECK(ruby(*engine, "Scene.find('Head').morph(:Frown).nil?") == "true");
  CHECK(ruby(*engine, "begin; Scene.find('Head').morph(:Frown, 1); rescue ArgumentError; 'refused'; end") == "refused");
  CHECK(ruby(*engine, "Scene.find('Camera').morph_names.empty?") == "true");
}

TEST_CASE("a clip's weights channel moves the shape keys as it plays") {
  const TempGame game(kShooterGame);
  const SceneDesc desc = face_under_a_head();
  auto engine = engine_for(game, desc);
  ruby(*engine, "Animation.pause(:Talk); Animation.seek(:Talk, " + std::to_string(kHalfway) + ")");
  frames(*engine, 1);
  // Talk goes from (0, 0) to (1, 0.5) in a second.
  CHECK(face(*engine).morph_weights[kSmile] == doctest::Approx(0.5f));
  CHECK(face(*engine).morph_weights[kWide] == doctest::Approx(0.25f));
}
