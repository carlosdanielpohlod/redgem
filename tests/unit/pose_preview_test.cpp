// Pose preview: which objects a clip poses, which clips an object shows, and the pose laid on a
// copy of the scene while the scene's own description keeps its rest pose.

#include <doctest/doctest.h>

#include <cmath>
#include <set>
#include <string>
#include <vector>

#include "editor/pose_preview.hpp"
#include "scene/animation/animation.hpp"
#include "scene/gltf/gltf_loader.hpp"
#include "test_support.hpp"

using namespace redgem;
using namespace redgem::pose_preview;
using redgem::test::fixture;

namespace {

constexpr int kWave = 0;
constexpr float kLater = 1.0f;    // seconds: the clip's last key
constexpr float kTooLate = 5.0f;  // past the clip's end
constexpr float kTolerance = 1e-4f;

SceneDesc skinned() {
  const LoadResult loaded = load_gltf_scene(fixture("skinned.glb"));
  REQUIRE_MESSAGE(loaded.ok, loaded.error);
  return loaded.scene;
}

float alike(Quat a, Quat b) { return std::abs(a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w); }

bool same_place(const Transform& a, const Transform& b) {
  const bool placed = length(a.position - b.position) < kTolerance && length(a.scale - b.scale) < kTolerance;
  return placed && alike(a.rotation, b.rotation) > 1.0f - kTolerance;
}

}  // namespace

TEST_CASE("a pose moves an object, what is under it, and the bones its meshes are skinned to") {
  const SceneDesc desc = skinned();
  const int arm = desc.find_node("Arm");
  const std::set<int> nodes = posed_nodes(desc, arm);
  CHECK(nodes.count(arm) == 1);
  CHECK(nodes.count(desc.find_node("Lower")) == 1);  // (beside the mesh, not under it)
  CHECK(nodes.count(desc.find_node("Upper")) == 1);
  CHECK(nodes.count(desc.find_node("Camera")) == 0);
  const std::set<int> rig = posed_nodes(desc, desc.find_node("Rig"));
  CHECK(rig.count(desc.find_node("Upper")) == 1);
  CHECK(posed_nodes(desc, kNoIndex).empty());
}

TEST_CASE("an object shows the clips that move it, by name") {
  const SceneDesc desc = skinned();
  CHECK(clips_moving(desc, desc.find_node("Rig")) == std::vector<int>{kWave});
  CHECK(clips_moving(desc, desc.find_node("Arm")) == std::vector<int>{kWave});
  CHECK(clips_moving(desc, desc.find_node("Camera")).empty());
  CHECK(clip_name(desc, kWave) == "Wave");
  SceneDesc unnamed = desc;
  unnamed.animations[0].name.clear();
  CHECK(clip_name(unnamed, kWave) == "Clip 0");
}

TEST_CASE("a pose is laid on the scene's copy at the time chosen, holding the last pose past the end") {
  const SceneDesc desc = skinned();
  const int upper = desc.find_node("Upper");  // (the bone the clip turns)
  Scene scene = Scene::from_desc(desc);
  const Transform rest = scene.at(upper).local;
  const Pose pose{desc.find_node("Rig"), kWave, kLater};
  REQUIRE(fits(desc, pose));
  show(desc, pose, scene);
  const Transform posed = scene.at(upper).local;
  CHECK_FALSE(same_place(posed, rest));
  CHECK(same_place(desc.nodes[static_cast<std::size_t>(upper)].local, rest));  // (the description is untouched)
  Scene late = Scene::from_desc(desc);
  show(desc, Pose{pose.object, kWave, kTooLate}, late);
  CHECK(same_place(late.at(upper).local, posed));
  CHECK(within_clip(desc.animations[0], kTooLate) == doctest::Approx(desc.animations[0].duration));
  CHECK(within_clip(desc.animations[0], -kLater) == 0.0f);
}

TEST_CASE("a pose whose clip does not move its object, or whose object is gone, is not shown") {
  const SceneDesc desc = skinned();
  CHECK_FALSE(fits(desc, Pose{desc.find_node("Camera"), kWave, 0.0f}));
  CHECK_FALSE(fits(desc, Pose{static_cast<int>(desc.nodes.size()), kWave, 0.0f}));
  CHECK_FALSE(fits(desc, Pose{desc.find_node("Rig"), 1, 0.0f}));
  Scene scene = Scene::from_desc(desc);
  const Transform rest = scene.at(desc.find_node("Upper")).local;
  show(desc, Pose{desc.find_node("Camera"), kWave, kLater}, scene);
  CHECK(same_place(scene.at(desc.find_node("Upper")).local, rest));
}
