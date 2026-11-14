#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>

#include "editor/document/glb_file.hpp"
#include "scene/gltf/gltf_loader.hpp"
#include "scene/props.hpp"
#include "scene/scene_ids.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::fixture;
using redgem::test::kTolerance;

TEST_CASE("loads the Blender hierarchy fixture with extras") {
  const LoadResult result = load_gltf_scene(fixture("hierarchy.glb"));
  REQUIRE_MESSAGE(result.ok, result.error);
  const SceneDesc& scene = result.scene;

  const int root = scene.find_node("Root");
  const int child = scene.find_node("Child");
  REQUIRE(root != kNoIndex);
  REQUIRE(child != kNoIndex);
  CHECK(root < child);  // parents come first
  CHECK(scene.nodes[child].parent == root);
  CHECK(scene.nodes[root].children == std::vector<int>{child});

  // Blender (1, 2, 3) with Z up becomes (1, 3, -2) with Y up.
  CHECK(approx_equal(scene.nodes[root].local.position, Vec3{1, 3, -2}, kTolerance));
  CHECK(approx_equal(scene.nodes[child].local.scale, Vec3{2, 2, 2}, kTolerance));

  const PropertyMap& props = scene.nodes[root].props;
  CHECK(get_string(props, "script") == "spinner.rb");
  CHECK(get_number(props, "speed") == doctest::Approx(2.5));
  CHECK(get_bool(props, "enabled") == true);
  CHECK(get_string(props, "label") == "hello");
  CHECK(get_number(props, "count") == doctest::Approx(3.0));
}

TEST_CASE("loads meshes, materials, camera and light") {
  const LoadResult result = load_gltf_scene(fixture("hierarchy.glb"));
  REQUIRE(result.ok);
  const SceneDesc& scene = result.scene;
  const NodeDesc& child = scene.nodes[scene.find_node("Child")];
  REQUIRE(child.mesh != kNoIndex);
  const MeshDesc& mesh = scene.meshes[child.mesh];
  REQUIRE(mesh.bounds.valid);
  CHECK(approx_equal(mesh.bounds.half_extents(), Vec3{0.5f, 0.5f, 0.5f}, kTolerance));
  REQUIRE_FALSE(mesh.primitives.empty());
  const PrimitiveDesc& primitive = mesh.primitives[0];
  CHECK(primitive.normals.size() == primitive.positions.size());
  CHECK(primitive.indices.size() % 3 == 0);
  REQUIRE(primitive.material != kNoIndex);
  CHECK(scene.materials[primitive.material].name == "Red");

  const NodeDesc& camera = scene.nodes[scene.find_node("Camera")];
  REQUIRE(camera.camera.has_value());
  CHECK(camera.camera->fov_y_radians > 0.0f);
  const NodeDesc& sun = scene.nodes[scene.find_node("Sun")];
  REQUIRE(sun.light.has_value());
  CHECK(sun.light->type == LightType::Directional);
  CHECK(sun.light->intensity == doctest::Approx(3.0f));
}

TEST_CASE("a spot light's cones are read from the file, and a light's cast_shadow from its extras") {
  constexpr float kInner = 0.25f;  // radians, as the file below says
  constexpr float kOuter = 0.5f;
  const std::filesystem::path file =
      std::filesystem::temp_directory_path() / ("redgem-spot-" + make_scene_id() + ".glb");
  GlbFile glb;
  glb.json = nlohmann::json::parse(R"({
    "asset": {"version": "2.0"}, "scene": 0, "scenes": [{"nodes": [0, 1]}],
    "extensionsUsed": ["KHR_lights_punctual"],
    "extensions": {"KHR_lights_punctual": {"lights": [
      {"type": "spot", "spot": {"innerConeAngle": 0.25, "outerConeAngle": 0.5}},
      {"type": "spot"}]}},
    "nodes": [
      {"name": "Spot", "extensions": {"KHR_lights_punctual": {"light": 0}}, "extras": {"cast_shadow": true}},
      {"name": "Plain", "extensions": {"KHR_lights_punctual": {"light": 1}}}]})");
  std::string error;
  REQUIRE_MESSAGE(write_glb(file, glb, error), error);
  const LoadResult result = load_gltf_scene(file.string());
  std::filesystem::remove(file);
  REQUIRE_MESSAGE(result.ok, result.error);
  const NodeDesc& spot = result.scene.nodes[result.scene.find_node("Spot")];
  REQUIRE(spot.light.has_value());
  CHECK(spot.light->type == LightType::Spot);
  CHECK(spot.light->inner_cone == doctest::Approx(kInner));
  CHECK(spot.light->outer_cone == doctest::Approx(kOuter));
  CHECK(get_bool(spot.props, props::kCastShadow) == true);
  // Without cones in the file, glTF's: full strength nowhere but the axis, gone a quarter turn off.
  const NodeDesc& plain = result.scene.nodes[result.scene.find_node("Plain")];
  REQUIRE(plain.light.has_value());
  CHECK(plain.light->inner_cone == doctest::Approx(0.0f));
  CHECK(plain.light->outer_cone == doctest::Approx(kDefaultSpotOuterCone));
}

TEST_CASE("missing files report an error") {
  const LoadResult result = load_gltf_scene(fixture("does_not_exist.glb"));
  CHECK_FALSE(result.ok);
  CHECK(result.error.find("file not found") != std::string::npos);
}

TEST_CASE("skeletons, vertex weights and animations are loaded") {
  constexpr float kWaveSeconds = 1.0f;
  const LoadResult loaded = load_gltf_scene(fixture("skinned.glb"));
  REQUIRE_MESSAGE(loaded.ok, loaded.error);
  const SceneDesc& scene = loaded.scene;
  const int arm = scene.find_node("Arm");
  REQUIRE(arm != kNoIndex);
  REQUIRE(scene.nodes[arm].skin == 0);
  REQUIRE(scene.skins.size() == 1);
  const SkinDesc& skin = scene.skins[0];
  REQUIRE(skin.joints.size() == 2);
  CHECK(skin.inverse_bind.size() == 2);
  // Joints point at the bone nodes, wherever the loader put them.
  CHECK(scene.nodes[skin.joints[0]].name == "Lower");
  CHECK(scene.nodes[skin.joints[1]].name == "Upper");

  const PrimitiveDesc& primitive = scene.meshes[scene.nodes[arm].mesh].primitives.front();
  const std::size_t vertices = primitive.positions.size() / 3;
  REQUIRE(primitive.joints.size() == vertices * 4);
  REQUIRE(primitive.weights.size() == vertices * 4);
  for (std::size_t v = 0; v < vertices; ++v) {
    const float total = primitive.weights[v * 4] + primitive.weights[v * 4 + 1] + primitive.weights[v * 4 + 2] +
                        primitive.weights[v * 4 + 3];
    CHECK(total == doctest::Approx(1.0f));
    CHECK(primitive.joints[v * 4] < skin.joints.size());
  }

  REQUIRE(scene.animations.size() == 1);
  const AnimationDesc& wave = scene.animations[0];
  CHECK(wave.name == "Wave");
  CHECK(wave.duration == doctest::Approx(kWaveSeconds));
  const bool turns_upper =
      std::any_of(wave.channels.begin(), wave.channels.end(), [&](const AnimationChannel& channel) {
        return channel.path == AnimationPath::Rotation && scene.nodes[channel.node].name == "Upper";
      });
  CHECK(turns_upper);
  // Rigid scenes have neither.
  const LoadResult rigid = load_gltf_scene(fixture("hierarchy.glb"));
  CHECK(rigid.scene.skins.empty());
  CHECK(rigid.scene.animations.empty());
}

TEST_CASE("a clip's markers are read from the redgem_events property of its extras") {
  constexpr std::size_t kFloats = 8;  // two key times, then two positions
  const std::array<float, kFloats> keys{0, 1, 0, 0, 0, 1, 0, 0};
  const std::filesystem::path file =
      std::filesystem::temp_directory_path() / ("redgem-markers-" + make_scene_id() + ".glb");
  GlbFile glb;
  glb.bin.resize(sizeof(keys));
  std::memcpy(glb.bin.data(), keys.data(), sizeof(keys));
  glb.json = nlohmann::json::parse(R"({
    "asset": {"version": "2.0"}, "scene": 0, "scenes": [{"nodes": [0]}],
    "nodes": [{"name": "Hips"}],
    "buffers": [{"byteLength": 32}],
    "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": 8}, {"buffer": 0, "byteOffset": 8, "byteLength": 24}],
    "accessors": [
      {"bufferView": 0, "componentType": 5126, "count": 2, "type": "SCALAR", "min": [0], "max": [1]},
      {"bufferView": 1, "componentType": 5126, "count": 2, "type": "VEC3"}],
    "animations": [{
      "name": "Walk",
      "channels": [{"sampler": 0, "target": {"node": 0, "path": "translation"}}],
      "samplers": [{"input": 0, "output": 1}],
      "extras": {"redgem_events": "footstep@0.25, footstep@0.75, oops@2"}}]})");
  std::string error;
  REQUIRE_MESSAGE(write_glb(file, glb, error), error);
  const LoadResult result = load_gltf_scene(file.string());
  std::filesystem::remove(file);
  REQUIRE_MESSAGE(result.ok, result.error);
  REQUIRE(result.scene.animations.size() == 1);
  const std::vector<AnimationMarker>& markers = result.scene.animations[0].markers;
  REQUIRE(markers.size() == 2);
  CHECK(markers[0].name == "footstep");
  CHECK(markers[0].at == doctest::Approx(0.25f));
  CHECK(markers[1].at == doctest::Approx(0.75f));
  const bool warned = std::any_of(result.warnings.begin(), result.warnings.end(), [](const std::string& warning) {
    return warning.find("oops@2") != std::string::npos;
  });
  CHECK(warned);
}
