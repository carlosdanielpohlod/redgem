#include <doctest/doctest.h>

#include <set>
#include <string>
#include <vector>

#include "editor/gltf_json.hpp"
#include "test_support.hpp"

using namespace redgem;
using namespace redgem::gltf_json;
using nlohmann::json;
using redgem::test::kTolerance;

namespace {

constexpr Vec3 kPosition{1.0f, 2.0f, 3.0f};
constexpr Vec3 kDoubleSize{2.0f, 2.0f, 2.0f};
constexpr int kMeshIndex = 4;
constexpr int kLightIndex = 2;
constexpr int kCameraIndex = 3;
constexpr int kFirstChild = 7;
constexpr int kSecondChild = 8;
constexpr double kSpeed = 2.5;
constexpr int kStaleExtra = 99;  // a scalar extra the node no longer has as a property
constexpr float kMetallic = 0.25f;
constexpr float kRoughness = 0.75f;
constexpr Color kOrange{1.0f, 0.5f, 0.0f, 1.0f};
constexpr Vec3 kBrightGlow{4.0f, 2.0f, 0.0f};  // four times what a glTF factor can say
constexpr float kBrightStrength = 4.0f;
constexpr Vec3 kBrightFactor{1.0f, 0.5f, 0.0f};
constexpr Vec3 kDimGlow{0.5f, 0.25f, 0.0f};
constexpr float kNoExtraStrength = 1.0f;
constexpr float kLampWatts = 500.0f;
constexpr float kLampRange = 12.0f;  // meters
constexpr float kNoRange = 0.0f;
constexpr float kFovRadians = 0.9f;
constexpr float kNear = 0.1f;  // meters
constexpr float kFar = 250.0f;
constexpr float kInfiniteFar = 0.0f;
constexpr int kNodesLoaded = 4;  // nodes a file had when it was read, in the skin tests
constexpr int kDeletedNode = 2;
constexpr int kNeverLoaded = 9;  // a number no node of the file had
constexpr int kMovedTo = 5;      // where node 1 of the file is now
constexpr int kActiveScene = 1;
constexpr const char* kOtherExtension = "KHR_other";
constexpr const char* kDoubleSided = "doubleSided";
constexpr const char* kSpeedProp = "speed";
constexpr const char* kLabelProp = "label";
constexpr const char* kLockedProp = "locked";
constexpr const char* kNestedExtra = "nested";
constexpr const char* kStaleProp = "old";
constexpr const char* kPath = "path";  // glTF: which property of its node a channel animates

// Where each node of a loaded file is now: 0 stayed, 1 moved, 2 was deleted, 3 stayed.
std::vector<int> current_nodes() { return {0, kMovedTo, kNoIndex, kNodesLoaded - 1}; }

NodeDesc placed_node() {
  NodeDesc node;
  node.name = "Crate";
  node.local.position = kPosition;
  node.local.scale = kDoubleSize;
  return node;
}

Vec3 vec3_of(const json& list) { return {list[0].get<float>(), list[1].get<float>(), list[2].get<float>()}; }

}  // namespace

TEST_CASE("a new document is an empty glTF scene") {
  const json document = empty_document();
  CHECK(document[kAsset][kVersion] == kGltfVersion);
  CHECK(document[kNodes].empty());
  REQUIRE(document[kScenes].size() == 1);
  CHECK(document[kScenes][0][kNodes].empty());
  CHECK(document[kScene] == 0);
}

TEST_CASE("properties are saved as flags, numbers and texts") {
  CHECK(property_json(PropertyValue(true)).is_boolean());
  CHECK(property_json(PropertyValue(kSpeed)) == kSpeed);
  CHECK(property_json(PropertyValue(std::string("door"))) == "door");
}

TEST_CASE("a node is saved with its name, placement and links") {
  NodeDesc node = placed_node();
  node.mesh = kMeshIndex;
  node.source_camera = kCameraIndex;
  node.source_light = kLightIndex;
  node.children = {kFirstChild, kSecondChild};
  const json saved = node_json(node, json::object());
  CHECK(saved[kName] == "Crate");
  CHECK(approx_equal(vec3_of(saved[kTranslation]), kPosition, kTolerance));
  CHECK(approx_equal(vec3_of(saved[kScale]), kDoubleSize, kTolerance));
  CHECK(saved[kRotation] == json({0.0f, 0.0f, 0.0f, 1.0f}));
  CHECK(saved[kMesh] == kMeshIndex);
  CHECK(saved[kCamera] == kCameraIndex);
  CHECK(saved[kExtensions][kLightsExtension][kLight] == kLightIndex);
  CHECK(saved[kChildren] == json({kFirstChild, kSecondChild}));
}

TEST_CASE("a node without mesh, camera, children or properties is saved without those fields") {
  const json original = {{kMesh, kMeshIndex}, {kCamera, kCameraIndex}, {kChildren, {kFirstChild}}};
  const json saved = node_json(placed_node(), original);
  CHECK_FALSE(saved.contains(kMesh));
  CHECK_FALSE(saved.contains(kCamera));
  CHECK_FALSE(saved.contains(kChildren));
  CHECK_FALSE(saved.contains(kExtras));
  CHECK_FALSE(saved.contains(kExtensions));
}

TEST_CASE("saving a node keeps the fields the editor does not edit and replaces its matrix") {
  const json original = {
      {kMatrix, json::array()}, {kWeights, {kSpeed}}, {kExtensions, {{kOtherExtension, json::object()}}}};
  const json saved = node_json(placed_node(), original);
  CHECK_FALSE(saved.contains(kMatrix));
  CHECK(saved[kWeights] == json({kSpeed}));
  CHECK(saved[kExtensions].contains(kOtherExtension));
}

TEST_CASE("a node that lost its light loses the light extension, and an empty extensions object with it") {
  const json light_only = {{kExtensions, {{kLightsExtension, {{kLight, kLightIndex}}}}}};
  CHECK_FALSE(node_json(placed_node(), light_only).contains(kExtensions));

  json with_another = light_only;
  with_another[kExtensions][kOtherExtension] = json::object();
  const json saved = node_json(placed_node(), with_another);
  CHECK_FALSE(saved[kExtensions].contains(kLightsExtension));
  CHECK(saved[kExtensions].contains(kOtherExtension));
}

TEST_CASE("scalar extras mirror the properties, and extras the editor cannot show are kept") {
  const json original = {{kExtras, {{kStaleProp, kStaleExtra}, {kNestedExtra, {{kSpeedProp, kSpeed}}}}}};
  NodeDesc node = placed_node();
  node.props[kSpeedProp] = kSpeed;
  node.props[kLabelProp] = std::string("door");
  node.props[kLockedProp] = true;
  const json extras = node_json(node, original)[kExtras];
  CHECK_FALSE(extras.contains(kStaleProp));
  CHECK(extras[kNestedExtra][kSpeedProp] == kSpeed);
  CHECK(extras[kSpeedProp] == kSpeed);
  CHECK(extras[kLabelProp] == "door");
  CHECK(extras[kLockedProp] == true);
}

TEST_CASE("a material is saved over its original, which keeps what the editor does not edit") {
  MaterialDesc material;
  material.name = "Paint";
  material.base_color = kOrange;
  material.metallic = kMetallic;
  material.roughness = kRoughness;
  const json saved = material_json(material, {{kName, "Old"}, {kDoubleSided, true}});
  CHECK(saved[kName] == "Paint");
  CHECK(saved[kDoubleSided] == true);
  CHECK(saved[kPbr][kBaseColorFactor] == json({kOrange.r, kOrange.g, kOrange.b, kOrange.a}));
  CHECK(saved[kPbr][kMetallicFactor] == kMetallic);
  CHECK(saved[kPbr][kRoughnessFactor] == kRoughness);
}

TEST_CASE("a glow brighter than a glTF factor allows is saved as a factor and a strength") {
  MaterialDesc bright;
  bright.emissive = kBrightGlow;
  CHECK(emissive_strength(bright) == doctest::Approx(kBrightStrength));
  CHECK(needs_emissive_strength(bright));
  const json saved = material_json(bright, json::object());
  CHECK(approx_equal(vec3_of(saved[kEmissiveFactor]), kBrightFactor, kTolerance));
  CHECK(saved[kExtensions][kEmissiveStrengthExtension][kEmissiveStrength] == kBrightStrength);

  MaterialDesc dim;
  dim.emissive = kDimGlow;
  CHECK(emissive_strength(dim) == doctest::Approx(kNoExtraStrength));
  CHECK_FALSE(needs_emissive_strength(dim));
  const json dimmed = material_json(dim, saved);  // the strength it had is taken away
  CHECK(approx_equal(vec3_of(dimmed[kEmissiveFactor]), kDimGlow, kTolerance));
  CHECK_FALSE(dimmed[kExtensions].contains(kEmissiveStrengthExtension));
}

TEST_CASE("edited materials are written over the file's and new ones are added after them") {
  MaterialDesc edited;
  edited.name = "Edited";
  MaterialDesc created;
  created.name = "Created";
  const json existing = json::array({{{kName, "Loaded"}, {kDoubleSided, true}}});
  const json materials = materials_json(existing, {edited, created});
  REQUIRE(materials.size() == 2);
  CHECK(materials[0][kName] == "Edited");
  CHECK(materials[0][kDoubleSided] == true);
  CHECK(materials[1][kName] == "Created");
  CHECK(materials_json(json::array(), {}).empty());
}

TEST_CASE("an extension is listed as used only once") {
  json document = json::object();
  add_extension_used(document, kLightsExtension);
  add_extension_used(document, kOtherExtension);
  add_extension_used(document, kLightsExtension);
  CHECK(document[kExtensionsUsed] == json({kLightsExtension, kOtherExtension}));
}

TEST_CASE("empty lists are removed instead of written") {
  json object = {{kSkins, {kFirstChild}}};
  set_unless_empty(object, kSkins, json::array());
  CHECK_FALSE(object.contains(kSkins));
  set_unless_empty(object, kSkins, json({kSecondChild}));
  CHECK(object[kSkins] == json({kSecondChild}));
}

TEST_CASE("lights are saved by type, and with a range only when they have one") {
  CHECK(std::string(light_type_name(LightType::Directional)) == kDirectionalLight);
  CHECK(std::string(light_type_name(LightType::Point)) == kPointLight);
  CHECK(std::string(light_type_name(LightType::Spot)) == kSpotLight);

  json entry = {{kName, "Lamp"}};
  write_light(entry, LightDesc{LightType::Point, kOrange, kLampWatts, kLampRange});
  CHECK(entry[kName] == "Lamp");
  CHECK(entry[kType] == kPointLight);
  CHECK(entry[kColor] == json({kOrange.r, kOrange.g, kOrange.b}));
  CHECK(entry[kIntensity] == kLampWatts);
  CHECK(entry[kRange] == kLampRange);
  write_light(entry, LightDesc{LightType::Spot, kOrange, kLampWatts, kNoRange});
  CHECK(entry[kType] == kSpotLight);
  CHECK_FALSE(entry.contains(kRange));
}

TEST_CASE("a camera without a far plane is saved as an infinite projection") {
  json entry = json::object();
  write_camera(entry, CameraDesc{kFovRadians, kNear, kFar});
  CHECK(entry[kType] == kPerspectiveCamera);
  CHECK(entry[kPerspective][kYfov] == kFovRadians);
  CHECK(entry[kPerspective][kZnear] == kNear);
  CHECK(entry[kPerspective][kZfar] == kFar);
  write_camera(entry, CameraDesc{kFovRadians, kNear, kInfiniteFar});
  CHECK_FALSE(entry[kPerspective].contains(kZfar));
}

TEST_CASE("a node keeps its own entry, a duplicate gets a copy and an unknown one a new entry") {
  json entries = json::array({{{kName, "Sun"}}});
  std::set<int> claimed;
  CHECK(claim_entry(entries, 0, claimed) == 0);
  CHECK(entries.size() == 1);

  const int copy = claim_entry(entries, 0, claimed);  // a second node made from the same entry
  CHECK(copy == 1);
  CHECK(entries[copy][kName] == "Sun");

  const int created = claim_entry(entries, kNoIndex, claimed);
  CHECK(created == copy + 1);
  CHECK(entries[created].empty());
  CHECK(claimed == std::set<int>{0, copy, created});

  json missing;  // a file without the list yet
  std::set<int> none;
  CHECK(claim_entry(missing, kNoIndex, none) == 0);
  CHECK(missing.is_array());
}

TEST_CASE("a node's number follows the node, and deleted or unknown nodes have none") {
  const std::vector<int> current = current_nodes();
  CHECK(current_index(current, 0) == 0);
  CHECK(current_index(current, 1) == kMovedTo);
  CHECK(current_index(current, kDeletedNode) == kNoIndex);
  CHECK(current_index(current, kNeverLoaded) == kNoIndex);
  CHECK(current_index(current, kNoIndex) == kNoIndex);
}

TEST_CASE("a skin follows its joints, and is dropped when one of them is gone") {
  const std::vector<int> current = current_nodes();
  const json skin = {{kJoints, {0, 1}}, {kSkeleton, 1}, {kInverseBindMatrices, kMeshIndex}};
  const auto followed = skin_with_current_nodes(skin, current);
  REQUIRE(followed.has_value());
  CHECK((*followed)[kJoints] == json({0, kMovedTo}));
  CHECK((*followed)[kSkeleton] == kMovedTo);
  CHECK((*followed)[kInverseBindMatrices] == kMeshIndex);

  const json rootless = {{kJoints, {0, 1}}, {kSkeleton, kDeletedNode}};
  const auto kept = skin_with_current_nodes(rootless, current);
  REQUIRE(kept.has_value());
  CHECK_FALSE(kept->contains(kSkeleton));

  const json broken = {{kJoints, {0, kDeletedNode}}};
  CHECK_FALSE(skin_with_current_nodes(broken, current).has_value());
}

TEST_CASE("an animation loses the channels of deleted nodes, and goes when none is left") {
  const std::vector<int> current = current_nodes();
  const json moved_channel = {{kSampler, 0}, {kTarget, {{kNode, 1}, {kPath, kTranslation}}}};
  const json deleted_channel = {{kSampler, 1}, {kTarget, {{kNode, kDeletedNode}, {kPath, kRotation}}}};
  const json untargeted_channel = {{kSampler, 1}};
  const json animation = {{kName, "Walk"}, {kChannels, {moved_channel, deleted_channel, untargeted_channel}}};
  const auto kept = animation_with_current_nodes(animation, current);
  REQUIRE(kept.has_value());
  CHECK((*kept)[kName] == "Walk");
  REQUIRE((*kept)[kChannels].size() == 1);
  CHECK((*kept)[kChannels][0][kTarget][kNode] == kMovedTo);

  const json gone = {{kName, "Wave"}, {kChannels, {deleted_channel}}};
  CHECK_FALSE(animation_with_current_nodes(gone, current).has_value());
}

TEST_CASE("only the scene the engine loads is kept, with the new top-level nodes") {
  json document = {{kScene, kActiveScene},
                   {kScenes, {{{kName, "Unused"}}, {{kName, "Level"}, {kNodes, {kFirstChild}}}}}};
  CHECK(has_several_scenes(document));
  keep_active_scene(document, json({0, kMovedTo}));
  CHECK_FALSE(has_several_scenes(document));
  REQUIRE(document[kScenes].size() == 1);
  CHECK(document[kScenes][0][kName] == "Level");
  CHECK(document[kScenes][0][kNodes] == json({0, kMovedTo}));
  CHECK(document[kScene] == 0);

  json sceneless = json::object();
  CHECK_FALSE(has_several_scenes(sceneless));
  keep_active_scene(sceneless, json({0}));
  CHECK(sceneless[kScenes][0][kNodes] == json({0}));
}

TEST_CASE("a file's 'scene' that names none of its scenes, or is no number, keeps a scene made here") {
  constexpr int kNoSuchScene = 7;
  for (const json& scene : {json(-1), json(kNoSuchScene), json("first"), json(nullptr), json(1.5)}) {
    CAPTURE(scene.dump());
    json document = {{kScene, scene}, {kScenes, {{{kName, "Level"}}}}};
    keep_active_scene(document, json({0}));
    REQUIRE(document[kScenes].size() == 1);
    CHECK(document[kScenes][0][kNodes] == json({0}));
    CHECK_FALSE(document[kScenes][0].contains(kName));
    CHECK(document[kScene] == 0);
  }
  json not_a_list = {{kScene, 0}, {kScenes, "none"}};
  keep_active_scene(not_a_list, json({0}));
  CHECK(not_a_list[kScenes].size() == 1);
}

TEST_CASE("an index is a whole number of zero or more, whatever else a file holds in its place") {
  constexpr int kIndex = 12;
  CHECK(index_from(json(kIndex)) == kIndex);
  CHECK(index_from(json(0u)) == 0);
  for (const json& other :
       {json(-1), json(1.5), json("3"), json(nullptr), json(true), json({1}), json(kMaxIndex + 1)}) {
    CAPTURE(other.dump());
    CHECK_FALSE(index_from(other).has_value());
  }
}

TEST_CASE("a skin or an animation that names a node with something that is no index loses it, without an error") {
  const std::vector<int> current = {0, 1};
  CHECK_FALSE(skin_with_current_nodes({{kJoints, {0, "one"}}}, current).has_value());
  CHECK_FALSE(skin_with_current_nodes({{kJoints, {0, -1}}}, current).has_value());
  const json channel = {{kTarget, {{kNode, nullptr}}}};
  CHECK_FALSE(animation_with_current_nodes({{kChannels, {channel}}}, current).has_value());
}

namespace {

constexpr int kTwoEntries = 2;
constexpr int kAnIndex = 3;

}  // namespace

TEST_CASE("a file's lists read as lists whether it has them or not, and are made when written to") {
  const json file = {{kNodes, {1, 2}}, {kScene, 0}};
  CHECK(list_of(file, kNodes).size() == kTwoEntries);
  CHECK(list_of(file, kMeshes).empty());
  json written = file;
  array_at(written, kMeshes).push_back(json::object());
  array_at(written, kScene).push_back(1);  // a number where a list belongs becomes a list
  CHECK(written[kMeshes].size() == 1);
  CHECK(written[kScene] == json::array({1}));
  CHECK(last_index(written[kNodes]) == 1);
  CHECK(last_index(json::array()) == kNoIndex);
  CHECK(in_range(0, kTwoEntries));
  CHECK(in_range(1, kTwoEntries));
  CHECK_FALSE(in_range(kTwoEntries, kTwoEntries));
  CHECK_FALSE(in_range(kNoIndex, kTwoEntries));
}

TEST_CASE("an entry of a list is found by its number when it is an object") {
  const json objects = {{kNodes, {{{kName, "a"}}, 1}}};
  REQUIRE(entry_at(objects, kNodes, 0) != nullptr);
  CHECK((*entry_at(objects, kNodes, 0))[kName] == "a");
  CHECK(entry_at(objects, kNodes, 1) == nullptr);  // not an object
  CHECK(entry_at(objects, kNodes, kTwoEntries) == nullptr);
  CHECK(entry_at(objects, kNodes, "0") == nullptr);
  CHECK(entry_at(objects, kMeshes, 0) == nullptr);
  CHECK(entry_at(json::array(), kNodes, 0) == nullptr);
}

TEST_CASE("an index is written as a number, and taken out when there is none") {
  json node = {{kMesh, kAnIndex}};
  set_or_erase_index(node, kMesh, kNoIndex);
  CHECK_FALSE(node.contains(kMesh));
  set_or_erase_index(node, kSkin, kAnIndex);
  CHECK(node[kSkin] == kAnIndex);
}
