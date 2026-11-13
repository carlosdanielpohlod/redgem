// Shape keys (glTF morph targets): how they are read from a file, how a clip moves their weights,
// and how the renderer lays them out for the vertex shader (render/morph_shapes.hpp).
#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "render/mesh_pieces.hpp"
#include "render/morph_shapes.hpp"
#include "scene/animation/animation.hpp"
#include "scene/gltf/gltf_loader.hpp"
#include "scene/scene.hpp"
#include "test_support.hpp"

using namespace redgem;
using namespace redgem::render_math;
using redgem::test::fixture;
using redgem::test::kTolerance;

namespace {

// What tests/fixtures/morph.glb holds (see tools/assets/make_morph_fixture.py).
constexpr std::size_t kSmile = 0;
constexpr std::size_t kWide = 1;
constexpr std::size_t kTopRight = 2;  // the vertex both keys move
constexpr std::size_t kCorners = 4;
constexpr float kLift = 0.5f;
constexpr float kStretch = 0.5f;
constexpr float kNormalTurn = 0.25f;
constexpr float kSmileAtStart = 0.25f;
constexpr std::size_t kY = 1;

SceneDesc morph_fixture() {
  const LoadResult loaded = load_gltf_scene(fixture("morph.glb"));
  REQUIRE_MESSAGE(loaded.ok, loaded.error);
  return loaded.scene;
}

float y_of(const std::vector<float>& xyz, std::size_t vertex) { return xyz[vertex * 3 + kY]; }

// A part of `vertices` vertices in a row along x, with one shape key moving vertex v up by v.
PrimitiveDesc lifted_row(std::size_t vertices) {
  PrimitiveDesc part;
  MorphTarget lift;
  for (std::size_t v = 0; v < vertices; ++v) {
    part.positions.insert(part.positions.end(), {static_cast<float>(v), 0.0f, 0.0f});
    part.normals.insert(part.normals.end(), {0.0f, 0.0f, 1.0f});
    lift.positions.insert(lift.positions.end(), {0.0f, static_cast<float>(v), 0.0f});
  }
  part.targets = {lift};
  return part;
}

}  // namespace

TEST_CASE("a mesh's shape keys are read with their names, starting weights and deltas") {
  const SceneDesc scene = morph_fixture();
  REQUIRE(scene.meshes.size() == 1);
  const MeshDesc& mesh = scene.meshes.front();
  CHECK(mesh.target_names == std::vector<std::string>{"Smile", "Wide"});
  REQUIRE(mesh.target_weights.size() == 2);
  CHECK(mesh.target_weights[kSmile] == doctest::Approx(kSmileAtStart));
  CHECK(mesh.target_weights[kWide] == doctest::Approx(0.0f));
  const PrimitiveDesc& part = mesh.primitives.front();
  REQUIRE(part.targets.size() == 2);
  CHECK(y_of(part.targets[kSmile].positions, kTopRight) == doctest::Approx(kLift));
  CHECK(y_of(part.targets[kSmile].normals, kTopRight) == doctest::Approx(kNormalTurn));
  CHECK(part.targets[kWide].normals.empty());  // Wide leaves the normals as they are
  // The bounds hold the square with each key applied whole.
  CHECK(mesh.bounds.max.y == doctest::Approx(0.5f + kLift));
  CHECK(mesh.bounds.max.x == doctest::Approx(0.5f + kStretch));
}

TEST_CASE("an object starts with its mesh's shape keys, and a weights channel moves them") {
  const SceneDesc desc = morph_fixture();
  Scene scene = Scene::from_desc(desc);
  const GameObject& face = scene.at(0);
  CHECK(face.morph_names == std::vector<std::string>{"Smile", "Wide"});
  CHECK(face.morph_weights == std::vector<float>{kSmileAtStart, 0.0f});

  REQUIRE(desc.animations.size() == 1);
  const AnimationDesc& talk = desc.animations.front();
  REQUIRE(talk.channels.size() == 1);
  CHECK(talk.channels.front().path == AnimationPath::Weights);
  CHECK(talk.duration == doctest::Approx(1.0f));
  apply_animation(talk, 0.5f, scene);  // halfway from (0, 0) to (1, 0.5)
  CHECK(face.morph_weights[kSmile] == doctest::Approx(0.5f));
  CHECK(face.morph_weights[kWide] == doctest::Approx(0.25f));

  PoseOptions half;
  half.weight = 0.5f;  // half of the way from where they are to the clip's
  apply_animation(talk, 0.0f, scene, half);
  CHECK(face.morph_weights[kSmile] == doctest::Approx(0.25f));
}

TEST_CASE("weights are sampled between keys and laid over others by a share") {
  AnimationChannel channel;
  channel.path = AnimationPath::Weights;
  channel.times = {0.0f, 2.0f};
  channel.values = {0.0f, 1.0f, 1.0f, 0.0f};  // two keys of two shape keys
  CHECK(sample_weights(channel, 0.5f) == std::vector<float>{0.25f, 0.75f});
  CHECK(sample_weights(channel, 9.0f) == std::vector<float>{1.0f, 0.0f});  // held at the last key
  CHECK(sample_weights(AnimationChannel{}, 0.0f).empty());

  std::vector<float> weights{0.0f, 1.0f, 0.5f};
  lay_weights_over(weights, {1.0f, 0.0f}, 0.25f);  // a shorter list leaves the rest alone
  CHECK(weights[0] == doctest::Approx(0.25f));
  CHECK(weights[1] == doctest::Approx(0.75f));
  CHECK(weights[2] == doctest::Approx(0.5f));
  lay_weights_over(weights, {1.0f, 0.0f}, 2.0f);  // more than whole is whole
  CHECK(weights[0] == doctest::Approx(1.0f));
  CHECK(weights[1] == doctest::Approx(0.0f));
}

TEST_CASE("a copy shows its heaviest shape keys, at most the shader's number of them") {
  std::vector<float> weights(kMaxActiveMorphs + 3, 0.0f);
  for (std::size_t key = 0; key < weights.size(); ++key) weights[key] = 0.05f * static_cast<float>(key);
  weights[1] = -0.9f;                   // heavy the other way
  weights[2] = kSmallestWeight * 0.5f;  // too light to draw
  const std::vector<ActiveMorph> active = active_morphs(weights);
  REQUIRE(active.size() == kMaxActiveMorphs);
  CHECK(active.front().key == 1);
  CHECK(active.front().weight == doctest::Approx(-0.9f));
  CHECK(active[1].key == weights.size() - 1);  // then the heaviest of the rest
  for (const ActiveMorph& morph : active) CHECK(morph.key != 2);

  CHECK_FALSE(shows_morphs({0.0f, kSmallestWeight * 0.5f}));
  CHECK(shows_morphs({0.0f, 0.5f}));
  CHECK(active_morphs({}).empty());
}

TEST_CASE("the shape keys a copy shows go to the shader as (key, weight) pairs, two to a texel") {
  const MorphTexels texels = morph_texels({{3, 0.5f}, {7, 1.0f}, {1, 0.25f}});
  const std::vector<float> laid(texels.begin(), texels.end());
  std::vector<float> expected(kMorphTexels * kFloatsPerTexel, 0.0f);  // the rest: key 0, no weight
  expected[0] = 3.0f;
  expected[1] = 0.5f;
  expected[2] = 7.0f;
  expected[3] = 1.0f;
  expected[4] = 1.0f;
  expected[5] = 0.25f;
  CHECK(laid == expected);
}

TEST_CASE("a part's deltas are laid out key after key, a position and a normal texel each vertex") {
  constexpr std::size_t kVertices = 3;
  PrimitiveDesc part = lifted_row(kVertices);
  part.targets.push_back(part.targets.front());
  part.targets.back().normals.assign(kVertices * 3, 1.0f);
  std::vector<float> texels(kFloatsPerTexel, 9.0f);  // a texel of another part already there
  const float first = append_morph_deltas(texels, part, kMaxTexels);
  CHECK(first == doctest::Approx(1.0f));
  REQUIRE(texels.size() == kFloatsPerTexel * (1 + 2 * kVertices * kTexelsPerMorphVertex));
  const auto texel = [&texels](std::size_t index) {
    return std::vector<float>(texels.begin() + static_cast<std::ptrdiff_t>(index * kFloatsPerTexel),
                              texels.begin() + static_cast<std::ptrdiff_t>((index + 1) * kFloatsPerTexel));
  };
  // Key 0, vertex 2: lifted by 2, normal unturned; key 1, vertex 0: normal turned by (1, 1, 1).
  CHECK(texel(1 + 2 * kTexelsPerMorphVertex) == std::vector<float>{0.0f, 2.0f, 0.0f, 0.0f});
  CHECK(texel(1 + 2 * kTexelsPerMorphVertex + 1) == std::vector<float>{0.0f, 0.0f, 0.0f, 0.0f});
  CHECK(texel(1 + kVertices * kTexelsPerMorphVertex + 1) == std::vector<float>{1.0f, 1.0f, 1.0f, 0.0f});

  std::vector<float> full;
  CHECK(append_morph_deltas(full, part, kVertices) == doctest::Approx(kNoMorphs));  // no room
  CHECK(full.empty());
  CHECK(append_morph_deltas(full, PrimitiveDesc{}, kMaxTexels) == doctest::Approx(kNoMorphs));
}

TEST_CASE("a vertex moves by each shape key it shows, times its weight") {
  const PrimitiveDesc part = lifted_row(kCorners);
  CHECK(approx_equal(morphed_position(part, 3, {{0, 0.5f}}), Vec3{3.0f, 1.5f, 0.0f}, kTolerance));
  CHECK(approx_equal(morphed_position(part, 3, {{0, -1.0f}}), Vec3{3.0f, -3.0f, 0.0f}, kTolerance));
  CHECK(approx_equal(morphed_position(part, 2, {}), Vec3{2.0f, 0.0f, 0.0f}, kTolerance));
  CHECK(approx_equal(morphed_position(part, 2, {{5, 1.0f}}), Vec3{2.0f, 0.0f, 0.0f}, kTolerance));  // no such key
}

TEST_CASE("a part cut into pieces keeps each shape key for the vertices of each piece") {
  constexpr std::size_t kVertices = 6;
  PrimitiveDesc part = lifted_row(kVertices);
  part.indices = {0, 1, 2, 3, 4, 5};
  const std::vector<PrimitiveDesc> pieces = split_primitive(part, kCorners);
  REQUIRE(pieces.size() == 2);
  REQUIRE(pieces[1].targets.size() == 1);
  CHECK(pieces[1].targets[0].positions.size() == pieces[1].positions.size());
  CHECK(y_of(pieces[1].targets[0].positions, 0) == doctest::Approx(3.0f));  // the second piece starts at vertex 3
  CHECK(pieces[1].targets[0].normals.empty());
}
