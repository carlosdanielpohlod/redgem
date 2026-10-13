#include <doctest/doctest.h>

#include <filesystem>
#include <stdexcept>
#include <string>

#include "editor/gltf_json.hpp"
#include "editor/model_import.hpp"

using namespace redgem;
using namespace redgem::gltf_json;
using namespace redgem::model_import;
using nlohmann::json;

namespace {

// What the scene already holds in these tests: a different count of each thing, so a shift by
// the wrong one shows.
constexpr int kViewsInScene = 10;
constexpr int kAccessorsInScene = 20;
constexpr int kImagesInScene = 30;
constexpr int kSamplersInScene = 40;
constexpr int kTexturesInScene = 50;
constexpr int kMaterialsInScene = 60;
constexpr int kMeshesInScene = 70;
constexpr int kNodesInFile = 80;
constexpr int kSkinsInFile = 90;
constexpr std::size_t kBytesInScene = 1024;
constexpr int kSkinsInScene = 5;
constexpr int kNodesInScene = 12;
constexpr int kRoot = 11;  // the object an imported model goes under
constexpr int kLocal = 3;  // an index inside the model's own lists
constexpr int kOtherLocal = 4;
constexpr int kViewOffset = 256;  // bytes: where a buffer view starts in the model's own data
constexpr int kOtherBuffer = 2;
constexpr int kParts = 2;  // pieces of a mesh: one with a material, one without
constexpr const char* kNormalTexture = "normalTexture";
constexpr const char* kBaseColorTexture = "baseColorTexture";
constexpr const char* kClearcoat = "KHR_materials_clearcoat";
constexpr const char* kClearcoatTexture = "clearcoatTexture";
constexpr const char* kTexCoord = "texCoord";
constexpr const char* kWebpExtension = "EXT_texture_webp";
constexpr const char* kEmbeddedImage = "data:image/png;base64,AAAA";

JsonOffsets json_offsets() {
  JsonOffsets offsets;
  offsets.buffer_views = kViewsInScene;
  offsets.accessors = kAccessorsInScene;
  offsets.images = kImagesInScene;
  offsets.samplers = kSamplersInScene;
  offsets.textures = kTexturesInScene;
  offsets.materials = kMaterialsInScene;
  offsets.meshes = kMeshesInScene;
  offsets.nodes = kNodesInFile;
  offsets.skins = kSkinsInFile;
  offsets.bin = kBytesInScene;
  return offsets;
}

DescOffsets desc_offsets() {
  DescOffsets offsets;
  offsets.images = kImagesInScene;
  offsets.materials = kMaterialsInScene;
  offsets.meshes = kMeshesInScene;
  offsets.skins = kSkinsInScene;
  offsets.nodes = kNodesInScene;
  offsets.source_nodes = kNodesInFile;
  offsets.root = kRoot;
  return offsets;
}

}  // namespace

TEST_CASE("only a model that keeps its data in one buffer can be merged into a scene") {
  CHECK_FALSE(has_several_buffers(json::object()));
  CHECK_FALSE(has_several_buffers({{kBuffers, {json::object()}}}));
  CHECK(has_several_buffers({{kBuffers, {json::object(), json::object()}}}));
}

TEST_CASE("an index is shifted where there is one and not invented where there is none") {
  json object = {{kMesh, kLocal}};
  shift_index(object, kMesh, kMeshesInScene);
  shift_index(object, kSkin, kSkinsInFile);
  CHECK(object[kMesh] == kLocal + kMeshesInScene);
  CHECK_FALSE(object.contains(kSkin));
}

TEST_CASE("texture references shift everywhere in a material, extensions included") {
  json material = {{kPbr, {{kBaseColorTexture, {{kIndex, kLocal}, {kTexCoord, 1}}}}},
                   {kNormalTexture, {{kIndex, kOtherLocal}}},
                   {kExtensions, {{kClearcoat, {{kClearcoatTexture, {{kIndex, kLocal}}}}}}},
                   {kName, "Wood"}};
  const json imported = imported_material(material, json_offsets());
  CHECK(imported[kPbr][kBaseColorTexture][kIndex] == kLocal + kTexturesInScene);
  CHECK(imported[kPbr][kBaseColorTexture][kTexCoord] == 1);
  CHECK(imported[kNormalTexture][kIndex] == kOtherLocal + kTexturesInScene);
  CHECK(imported[kExtensions][kClearcoat][kClearcoatTexture][kIndex] == kLocal + kTexturesInScene);
  CHECK(imported[kName] == "Wood");
}

TEST_CASE("the extensions of a model are added to the scene's without repeats") {
  json scene = {{kExtensionsUsed, {kLightsExtension}}};
  const json model = {{kExtensionsUsed, {kLightsExtension, kClearcoat}}};
  merge_string_list(scene, model, kExtensionsUsed);
  merge_string_list(scene, model, kExtensionsRequired);
  CHECK(scene[kExtensionsUsed] == json({kLightsExtension, kClearcoat}));
  CHECK_FALSE(scene.contains(kExtensionsRequired));

  json bare = json::object();
  merge_string_list(bare, model, kExtensionsUsed);
  CHECK(bare[kExtensionsUsed] == json({kLightsExtension, kClearcoat}));
}

TEST_CASE("a buffer view moves to where the model's data lands in the scene's only buffer") {
  const json view = {{kBuffer, kOtherBuffer}, {kByteOffset, kViewOffset}, {kByteLength, kViewOffset}};
  const json imported = imported_buffer_view(view, json_offsets());
  CHECK(imported[kBuffer] == 0);
  CHECK(imported[kByteOffset] == kViewOffset + kBytesInScene);
  CHECK(imported[kByteLength] == kViewOffset);
  CHECK(imported_buffer_view(json::object(), json_offsets())[kByteOffset] == kBytesInScene);
}

TEST_CASE("accessors point at the shifted buffer views, sparse ones included") {
  const json accessor = {{kBufferView, kLocal},
                         {kSparse, {{kIndices, {{kBufferView, kLocal}}}, {kValues, {{kBufferView, kOtherLocal}}}}}};
  const json imported = imported_accessor(accessor, json_offsets());
  CHECK(imported[kBufferView] == kLocal + kViewsInScene);
  CHECK(imported[kSparse][kIndices][kBufferView] == kLocal + kViewsInScene);
  CHECK(imported[kSparse][kValues][kBufferView] == kOtherLocal + kViewsInScene);
}

TEST_CASE("an image file is pointed at from the scene's folder, and images inside the file are left alone") {
  const std::filesystem::path scene_dir = "/games/demo";
  const std::filesystem::path model_dir = scene_dir / "models";
  const json file = imported_image({{kUri, "wood.png"}}, json_offsets(), model_dir, scene_dir);
  CHECK(file[kUri] == "models/wood.png");

  const json embedded = imported_image({{kUri, kEmbeddedImage}}, json_offsets(), model_dir, scene_dir);
  CHECK(embedded[kUri] == kEmbeddedImage);

  const json in_buffer = imported_image({{kBufferView, kLocal}}, json_offsets(), model_dir, scene_dir);
  CHECK(in_buffer[kBufferView] == kLocal + kViewsInScene);
  CHECK_FALSE(in_buffer.contains(kUri));
}

TEST_CASE("textures point at the shifted samplers and images, also through their extensions") {
  const json texture = {
      {kSampler, kLocal}, {kSource, kOtherLocal}, {kExtensions, {{kWebpExtension, {{kSource, kLocal}}}}}};
  const json imported = imported_texture(texture, json_offsets());
  CHECK(imported[kSampler] == kLocal + kSamplersInScene);
  CHECK(imported[kSource] == kOtherLocal + kImagesInScene);
  CHECK(imported[kExtensions][kWebpExtension][kSource] == kLocal + kImagesInScene);
}

TEST_CASE("meshes point at the shifted accessors and materials, and lose their morph targets") {
  const json primitive = {{kAttributes, {{kPositionAttribute, kLocal}, {kNormalAttribute, kOtherLocal}}},
                          {kIndices, kLocal},
                          {kMaterial, kOtherLocal},
                          {kTargets, {json::object()}}};
  const json mesh = {
      {kPrimitives, {primitive}}, {kWeights, {0, 1}}, {kExtras, {{kTargetNames, {"smile"}}, {kName, "kept"}}}};
  const json imported = imported_mesh(mesh, json_offsets());
  const json& part = imported[kPrimitives][0];
  CHECK(part[kAttributes][kPositionAttribute] == kLocal + kAccessorsInScene);
  CHECK(part[kAttributes][kNormalAttribute] == kOtherLocal + kAccessorsInScene);
  CHECK(part[kIndices] == kLocal + kAccessorsInScene);
  CHECK(part[kMaterial] == kOtherLocal + kMaterialsInScene);
  CHECK_FALSE(part.contains(kTargets));
  CHECK_FALSE(imported.contains(kWeights));
  CHECK_FALSE(imported[kExtras].contains(kTargetNames));
  CHECK(imported[kExtras][kName] == "kept");
}

TEST_CASE("a model's nodes keep their mesh and skin, and come without camera, light or children") {
  const json node = {{kName, "Arm"},       {kMesh, kLocal},    {kSkin, kOtherLocal},
                     {kCamera, 0},         {kWeights, {0, 1}}, {kExtensions, {{kLightsExtension, {{kLight, 0}}}}},
                     {kChildren, {kLocal}}};
  const json imported = imported_node(node, json_offsets());
  CHECK(imported == json({{kName, "Arm"}, {kMesh, kLocal + kMeshesInScene}, {kSkin, kOtherLocal + kSkinsInFile}}));
}

TEST_CASE("skins and animations point at the shifted nodes and accessors") {
  const json skin = {{kJoints, {kLocal, kOtherLocal}}, {kSkeleton, kLocal}, {kInverseBindMatrices, kOtherLocal}};
  const json imported_bones = imported_skin(skin, json_offsets());
  CHECK(imported_bones[kJoints] == json({kLocal + kNodesInFile, kOtherLocal + kNodesInFile}));
  CHECK(imported_bones[kSkeleton] == kLocal + kNodesInFile);
  CHECK(imported_bones[kInverseBindMatrices] == kOtherLocal + kAccessorsInScene);

  const json animation = {{kSamplers, {{{kInput, kLocal}, {kOutput, kOtherLocal}}}},
                          {kChannels, {{{kSampler, 0}, {kTarget, {{kNode, kLocal}}}}, {{kSampler, 0}}}}};
  const json imported_walk = imported_animation(animation, json_offsets());
  CHECK(imported_walk[kSamplers][0][kInput] == kLocal + kAccessorsInScene);
  CHECK(imported_walk[kSamplers][0][kOutput] == kOtherLocal + kAccessorsInScene);
  CHECK(imported_walk[kChannels][0][kTarget][kNode] == kLocal + kNodesInFile);
  CHECK(imported_walk[kChannels][0][kSampler] == 0);  // samplers are the animation's own
  CHECK_FALSE(imported_walk[kChannels][1].contains(kTarget));
}

TEST_CASE("the description's materials and meshes are shifted, and what is absent stays absent") {
  MaterialDesc material;
  material.base_color_image = kLocal;
  material.normal_image = kOtherLocal;
  const MaterialDesc imported = imported_material(material, desc_offsets());
  CHECK(imported.base_color_image == kLocal + kImagesInScene);
  CHECK(imported.normal_image == kOtherLocal + kImagesInScene);
  CHECK(imported.emissive_image == kNoIndex);
  CHECK(imported.occlusion_image == kNoIndex);
  CHECK(imported.metallic_roughness_image == kNoIndex);

  MeshDesc mesh;
  mesh.primitives.resize(kParts);
  mesh.primitives[0].material = kLocal;
  const MeshDesc imported_parts = imported_mesh(mesh, desc_offsets());
  CHECK(imported_parts.primitives[0].material == kLocal + kMaterialsInScene);
  CHECK(imported_parts.primitives[1].material == kNoIndex);
}

TEST_CASE("an imported object goes under the model's root or keeps its parent, without camera or light") {
  NodeDesc top;
  top.children = {1};
  top.mesh = kLocal;
  top.skin = 0;
  top.source_node = kOtherLocal;
  top.camera = CameraDesc{};
  top.light = LightDesc{};
  top.source_camera = 0;
  top.source_light = 0;
  const NodeDesc imported_top = imported_node(top, desc_offsets());
  CHECK(imported_top.parent == kRoot);
  CHECK(imported_top.children == std::vector<int>{1 + kNodesInScene});
  CHECK(imported_top.mesh == kLocal + kMeshesInScene);
  CHECK(imported_top.skin == kSkinsInScene);
  CHECK(imported_top.source_node == kOtherLocal + kNodesInFile);
  CHECK_FALSE(imported_top.camera.has_value());
  CHECK_FALSE(imported_top.light.has_value());
  CHECK(imported_top.source_camera == kNoIndex);
  CHECK(imported_top.source_light == kNoIndex);

  NodeDesc child;
  child.parent = 0;
  const NodeDesc imported_child = imported_node(child, desc_offsets());
  CHECK(imported_child.parent == kNodesInScene);
  CHECK(imported_child.mesh == kNoIndex);
  CHECK(imported_child.skin == kNoIndex);
  CHECK(imported_child.source_node == kNoIndex);
}

TEST_CASE("skeleton joints and animation channels follow the imported objects") {
  SkinDesc skin;
  skin.joints = {0, kLocal, kNoIndex};
  CHECK(imported_skin(skin, desc_offsets()).joints ==
        std::vector<int>{kNodesInScene, kLocal + kNodesInScene, kNoIndex});

  AnimationDesc animation;
  animation.channels.resize(1);
  animation.channels[0].node = kLocal;
  CHECK(imported_animation(animation, desc_offsets()).channels[0].node == kLocal + kNodesInScene);
}

TEST_CASE("what a model holds in place of an index is taken out, not shifted and not an error") {
  for (const json& other : {json("3"), json(nullptr), json(-2), json(1.5), json({1, 2})}) {
    CAPTURE(other.dump());
    json object = {{kMesh, other}, {kName, "Crate"}};
    shift_index(object, kMesh, kMeshesInScene);
    CHECK_FALSE(object.contains(kMesh));
    CHECK(object[kName] == "Crate");
  }
  json not_an_object = json::array({1, 2});
  shift_index(not_an_object, kMesh, kMeshesInScene);
  CHECK(not_an_object == json::array({1, 2}));
  const json view = imported_buffer_view({{kByteOffset, -8}}, json_offsets());
  CHECK(view[kByteOffset] == kBytesInScene);
}

TEST_CASE("a mesh or a skeleton that points at nothing stops the import with an error to report") {
  const json mesh = {{kPrimitives, {{{kAttributes, {{kPositionAttribute, nullptr}}}}}}};
  CHECK_THROWS_AS(imported_mesh(mesh, json_offsets()), std::exception);
  CHECK_THROWS_AS(imported_skin({{kJoints, {0, "one"}}}, json_offsets()), std::exception);
}

TEST_CASE("a material nested very deep is walked a few levels and no further") {
  constexpr int kVeryDeep = 150000;
  json material = {{kNormalTexture, {{kIndex, kLocal}}}};
  for (int i = 0; i < kVeryDeep; ++i) {
    json outer = json::object();
    outer[kExtensions] = std::move(material);
    material = std::move(outer);
  }
  shift_texture_refs(material, kTexturesInScene);  // comes back
  CHECK(material.contains(kExtensions));
}

TEST_CASE("a model's picture outside the scene's folder does not come along, nor one that is no file name") {
  const std::filesystem::path scene_dir = "/games/demo";
  const std::filesystem::path model_dir = scene_dir / "models";
  for (const json& uri : {json("../../secret.png"), json("/etc/passwd"), json("textures/../../../x.png"), json(7)}) {
    CAPTURE(uri.dump());
    const json image = imported_image({{kUri, uri}, {kName, "Wood"}}, json_offsets(), model_dir, scene_dir);
    CHECK_FALSE(image.contains(kUri));
    CHECK(image[kName] == "Wood");
  }
  const json up_and_back = imported_image({{kUri, "../images/wood.png"}}, json_offsets(), model_dir, scene_dir);
  CHECK(up_and_back[kUri] == "images/wood.png");
}

namespace {

// A scene holding one of each thing, and a model holding one more of each.
json scene_with_one_of_each() {
  return {{kBufferViews, json::array({json::object()})}, {kAccessors, json::array({json::object()})},
          {kImages, json::array({json::object()})},      {kSamplers, json::array({json::object()})},
          {kTextures, json::array({json::object()})},    {kMaterials, json::array({json::object()})},
          {kMeshes, json::array({json::object()})},      {kExtensionsUsed, {kClearcoat}}};
}

json model_with_one_of_each() {
  return {{kBufferViews, {{{kBuffer, 0}, {kByteOffset, kViewOffset}}}},
          {kAccessors, {{{kBufferView, 0}}}},
          {kImages, {{{kUri, kEmbeddedImage}}}},
          {kSamplers, json::array({json::object()})},
          {kTextures, {{{kSource, 0}, {kSampler, 0}}}},
          {kMaterials, {{{kNormalTexture, {{kIndex, 0}}}}}},
          {kMeshes, {{{kPrimitives, {{{kAttributes, {{kPositionAttribute, 0}}}, {kMaterial, 0}}}}}}},
          {kExtensionsUsed, {kClearcoat, kWebpExtension}}};
}

JsonOffsets one_of_each() {
  JsonOffsets offsets;
  offsets.buffer_views = offsets.accessors = offsets.images = offsets.samplers = 1;
  offsets.textures = offsets.materials = offsets.meshes = 1;
  offsets.bin = kBytesInScene;
  return offsets;
}

}  // namespace

TEST_CASE("a model merged into a scene comes after the scene's own entries, pointing at its own") {
  json scene = scene_with_one_of_each();
  merge_model(scene, model_with_one_of_each(), one_of_each(), "models", "");
  for (const char* list : {kBufferViews, kAccessors, kImages, kSamplers, kTextures, kMaterials, kMeshes}) {
    CAPTURE(list);
    CHECK(scene[list].size() == 2);
  }
  CHECK(scene[kBufferViews][1][kByteOffset] == kViewOffset + kBytesInScene);
  CHECK(scene[kAccessors][1][kBufferView] == 1);
  CHECK(scene[kTextures][1][kSource] == 1);
  CHECK(scene[kTextures][1][kSampler] == 1);
  CHECK(scene[kMaterials][1][kNormalTexture][kIndex] == 1);
  CHECK(scene[kMeshes][1][kPrimitives][0][kMaterial] == 1);
  CHECK(scene[kMeshes][1][kPrimitives][0][kAttributes][kPositionAttribute] == 1);
  CHECK(scene[kExtensionsUsed] == json({kClearcoat, kWebpExtension}));
}

TEST_CASE("a model dropped again leaves the scene's lists as they were, extensions aside") {
  json scene = scene_with_one_of_each();
  merge_model(scene, model_with_one_of_each(), one_of_each(), "models", "");
  drop_model(scene, one_of_each());
  json expected = scene_with_one_of_each();
  expected[kExtensionsUsed] = {kClearcoat, kWebpExtension};
  CHECK(scene == expected);
  json empty;
  drop_model(empty, one_of_each());  // a scene without lists has nothing to cut
  CHECK(empty.is_null());
}

TEST_CASE("a model whose mesh points at nothing stops the merge with an error to report") {
  json scene = scene_with_one_of_each();
  json model = model_with_one_of_each();
  model[kMeshes][0][kPrimitives][0][kAttributes][kPositionAttribute] = "zero";
  CHECK_THROWS_AS(merge_model(scene, model, one_of_each(), "models", ""), std::invalid_argument);
}
