#include <doctest/doctest.h>

#include <filesystem>
#include <numeric>

#include "editor/document/glb_compact.hpp"
#include "editor/document/scene_document.hpp"
#include "scene/gltf/gltf_loader.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::fixture;

namespace {

constexpr std::size_t kBinBytes = 24;
constexpr int kImports = 3;
constexpr Color kGray{0.5f, 0.5f, 0.5f, 1.0f};

// Two meshes, each with a material, a texture, a picture and bytes of its own; one object, which
// shows the second mesh. The binary chunk is the bytes 0, 1, 2...
GlbFile two_meshes_one_shown() {
  GlbFile glb;
  glb.bin.resize(kBinBytes);
  std::iota(glb.bin.begin(), glb.bin.end(), std::uint8_t{0});
  glb.json = nlohmann::json::parse(R"({
    "asset": {"version": "2.0"},
    "buffers": [{"byteLength": 24}],
    "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": 8}, {"buffer": 0, "byteOffset": 8, "byteLength": 8},
                    {"buffer": 0, "byteOffset": 16, "byteLength": 3}, {"buffer": 0, "byteOffset": 20, "byteLength": 4}],
    "accessors": [{"bufferView": 0, "count": 2}, {"bufferView": 1, "count": 2}],
    "images": [{"bufferView": 2, "mimeType": "image/png"}, {"bufferView": 3, "mimeType": "image/png"}],
    "samplers": [{"magFilter": 9728}, {"magFilter": 9729}],
    "textures": [{"source": 0, "sampler": 0}, {"source": 1, "sampler": 1}],
    "materials": [{"name": "Gone", "pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}},
                  {"name": "Kept", "pbrMetallicRoughness": {"baseColorTexture": {"index": 1}},
                   "extensions": {"KHR_materials_clearcoat": {"clearcoatTexture": {"index": 1}}}}],
    "meshes": [{"name": "Gone", "primitives": [{"attributes": {"POSITION": 0}, "material": 0}]},
               {"name": "Kept", "primitives": [{"attributes": {"POSITION": 1}, "material": 1}]}],
    "nodes": [{"name": "Shown", "mesh": 1}],
    "scenes": [{"nodes": [0]}],
    "scene": 0
  })");
  return glb;
}

std::filesystem::path temp_scene(const std::string& name) {
  return std::filesystem::temp_directory_path() / ("redgem_compact_" + name + ".glb");
}

// A scene with a floor, saved.
SceneDocument saved_floor(const std::filesystem::path& file) {
  SceneDocument doc = SceneDocument::create_empty(file);
  doc.add_primitive(PrimitiveShape::Plane, "Floor", kGray);
  std::string error;
  REQUIRE_MESSAGE(doc.save(error), error);
  return doc;
}

}  // namespace

TEST_CASE("what no object uses is left out of a file, and what is left is numbered anew") {
  const std::optional<GlbFile> lean = without_unused(two_meshes_one_shown());
  REQUIRE(lean.has_value());
  const nlohmann::json& json = lean->json;
  for (const char* list : {"meshes", "materials", "textures", "images", "samplers", "accessors"}) {
    CAPTURE(list);
    CHECK(json[list].size() == 1);
  }
  CHECK(json["meshes"][0]["name"] == "Kept");
  CHECK(json["materials"][0]["name"] == "Kept");
  CHECK(json["samplers"][0]["magFilter"] == 9729);
  // Everything points at where what it used went.
  CHECK(json["nodes"][0]["mesh"] == 0);
  CHECK(json["meshes"][0]["primitives"][0]["attributes"]["POSITION"] == 0);
  CHECK(json["meshes"][0]["primitives"][0]["material"] == 0);
  CHECK(json["materials"][0]["pbrMetallicRoughness"]["baseColorTexture"]["index"] == 0);
  CHECK(json["materials"][0]["extensions"]["KHR_materials_clearcoat"]["clearcoatTexture"]["index"] == 0);
  CHECK(json["textures"][0]["source"] == 0);
  CHECK(json["textures"][0]["sampler"] == 0);
  CHECK(json["accessors"][0]["bufferView"] == 0);
  CHECK(json["images"][0]["bufferView"] == 1);
}

TEST_CASE("the bytes of what is left out go too, and the rest starts on multiples of four") {
  const std::optional<GlbFile> lean = without_unused(two_meshes_one_shown());
  REQUIRE(lean.has_value());
  const nlohmann::json& views = lean->json["bufferViews"];
  REQUIRE(views.size() == 2);
  CHECK(views[0]["byteOffset"] == 0);
  CHECK(views[1]["byteOffset"] == 8);
  // The second mesh's eight bytes, then its picture's four.
  const std::vector<std::uint8_t> expected = {8, 9, 10, 11, 12, 13, 14, 15, 20, 21, 22, 23};
  CHECK(lean->bin == expected);
  CHECK(lean->json["buffers"][0]["byteLength"] == expected.size());
}

TEST_CASE("a file whose every part is used, or that is not understood, is left whole") {
  GlbFile glb = two_meshes_one_shown();
  glb.json["nodes"].push_back({{"name", "Other"}, {"mesh", 0}});
  CHECK_FALSE(without_unused(glb).has_value());  // nothing to leave out

  // An extension may point at the lists in its own way: nothing is renumbered under it.
  GlbFile compressed = two_meshes_one_shown();
  compressed.json["meshes"][1]["primitives"][0]["extensions"]["KHR_draco_mesh_compression"] = {{"bufferView", 0}};
  CHECK_FALSE(without_unused(compressed).has_value());
  GlbFile instanced = two_meshes_one_shown();
  instanced.json["nodes"][0]["extensions"]["EXT_mesh_gpu_instancing"] = {{"attributes", {{"TRANSLATION", 0}}}};
  CHECK_FALSE(without_unused(instanced).has_value());
  GlbFile lit = two_meshes_one_shown();
  lit.json["nodes"][0]["extensions"]["KHR_lights_punctual"] = {{"light", 0}};  // the editor's own: looked past
  lit.json["extensions"]["KHR_lights_punctual"]["lights"] = {{{"type", "point"}}};
  CHECK(without_unused(lit).has_value());

  // Something else where glTF wants an index, or data outside the file's bytes.
  GlbFile pointing_nowhere = two_meshes_one_shown();
  pointing_nowhere.json["nodes"][0]["mesh"] = 7;
  CHECK_FALSE(without_unused(pointing_nowhere).has_value());
  GlbFile not_an_index = two_meshes_one_shown();
  not_an_index.json["meshes"][1]["primitives"][0]["attributes"]["POSITION"] = "first";
  CHECK_FALSE(without_unused(not_an_index).has_value());
  GlbFile past_the_end = two_meshes_one_shown();
  past_the_end.json["bufferViews"][1]["byteLength"] = kBinBytes;
  CHECK_FALSE(without_unused(past_the_end).has_value());
  // (Looking never changes the file it looks at.)
  const GlbFile original = two_meshes_one_shown();
  without_unused(original);
  CHECK(original.json == two_meshes_one_shown().json);
}

TEST_CASE("the lights and cameras no object carries are left out, and the others are numbered anew") {
  GlbFile glb = two_meshes_one_shown();
  glb.json["cameras"] = {{{"name", "Gone"}, {"type", "perspective"}}, {{"name", "Kept"}, {"type", "perspective"}}};
  glb.json["extensions"]["KHR_lights_punctual"]["lights"] = {{{"name", "Gone"}, {"type", "point"}},
                                                             {{"name", "Also gone"}, {"type", "spot"}},
                                                             {{"name", "Kept"}, {"type", "point"}}};
  glb.json["nodes"][0]["camera"] = 1;
  glb.json["nodes"][0]["extensions"]["KHR_lights_punctual"] = {{"light", 2}};
  const std::optional<GlbFile> lean = without_unused(glb);
  REQUIRE(lean.has_value());
  const nlohmann::json& lights = lean->json["extensions"]["KHR_lights_punctual"]["lights"];
  REQUIRE(lights.size() == 1);
  CHECK(lights[0]["name"] == "Kept");
  REQUIRE(lean->json["cameras"].size() == 1);
  CHECK(lean->json["cameras"][0]["name"] == "Kept");
  CHECK(lean->json["nodes"][0]["camera"] == 0);
  CHECK(lean->json["nodes"][0]["extensions"]["KHR_lights_punctual"]["light"] == 0);

  // With no object carrying any, the lists go.
  glb.json["nodes"][0].erase("camera");
  glb.json["nodes"][0].erase("extensions");
  const std::optional<GlbFile> dark = without_unused(glb);
  REQUIRE(dark.has_value());
  CHECK_FALSE(dark->json.contains("cameras"));
  CHECK_FALSE(dark->json["extensions"]["KHR_lights_punctual"].contains("lights"));
}

TEST_CASE("a deleted light's and camera's entries leave the file, and come back with an undo") {
  const std::filesystem::path file = temp_scene("deleted_light");
  SceneDocument doc = saved_floor(file);
  std::string error;
  const auto lights_in_file = [&file, &error] {
    const std::optional<GlbFile> glb = read_glb(file, error);
    REQUIRE_MESSAGE(glb.has_value(), error);
    const nlohmann::json& extensions = glb->json.value("extensions", nlohmann::json::object());
    return extensions.value("KHR_lights_punctual", nlohmann::json::object())
        .value("lights", nlohmann::json::array())
        .size();
  };
  const auto cameras_in_file = [&file, &error] {
    return read_glb(file, error)->json.value("cameras", nlohmann::json::array()).size();
  };
  const std::size_t lights_before = lights_in_file();
  const std::size_t cameras_before = cameras_in_file();
  const int lamp = doc.add_light(LightType::Point, "Lamp", kGray, 1.0f);
  doc.add_light(LightType::Spot, "Spot", kGray, 1.0f);
  const int camera = doc.add_camera("Watcher", 1.0f);
  REQUIRE_MESSAGE(doc.save(error), error);
  REQUIRE(lights_in_file() == lights_before + 2);
  REQUIRE(cameras_in_file() == cameras_before + 1);
  doc.remove(camera);
  doc.remove(lamp);
  REQUIRE_MESSAGE(doc.save(error), error);
  CHECK(lights_in_file() == lights_before + 1);
  CHECK(cameras_in_file() == cameras_before);
  // The light left is still the spot's, and the file loads with it.
  const LoadResult loaded = load_gltf_scene(file.string());
  REQUIRE_MESSAGE(loaded.ok, loaded.error);
  const int spot = loaded.scene.find_node("Spot");
  REQUIRE(spot != kNoIndex);
  REQUIRE(loaded.scene.nodes[spot].light.has_value());
  CHECK(loaded.scene.nodes[spot].light->type == LightType::Spot);
  REQUIRE(doc.undo());
  REQUIRE(doc.undo());
  REQUIRE_MESSAGE(doc.save(error), error);
  CHECK(lights_in_file() == lights_before + 2);
  CHECK(cameras_in_file() == cameras_before + 1);
  std::filesystem::remove(file);
}

TEST_CASE("skeletons and animations keep their data, whichever object they move") {
  GlbFile glb = two_meshes_one_shown();
  glb.json["skins"] = {{{"joints", {0}}, {"inverseBindMatrices", 0}}};
  const std::optional<GlbFile> lean = without_unused(glb);
  REQUIRE(lean.has_value());
  // The first accessor is used again, by the skeleton: only the first mesh's looks are left out.
  CHECK(lean->json["accessors"].size() == 2);
  CHECK(lean->json["skins"][0]["inverseBindMatrices"] == 0);
  CHECK(lean->json["meshes"][0]["primitives"][0]["attributes"]["POSITION"] == 1);
  CHECK(lean->json["meshes"].size() == 1);
}

TEST_CASE("undoing a model import leaves nothing of the model in the saved scene, however often") {
  const std::filesystem::path file = temp_scene("undone_import");
  SceneDocument doc = saved_floor(file);
  const std::uintmax_t before = std::filesystem::file_size(file);
  std::string error;
  for (int i = 0; i < kImports; ++i) {
    REQUIRE_MESSAGE(doc.import_model(fixture("skinned.glb"), error) != kNoIndex, error);
    REQUIRE(doc.undo());
    REQUIRE_MESSAGE(doc.save(error), error);
    CHECK(std::filesystem::file_size(file) == before);
  }
  const LoadResult loaded = load_gltf_scene(file.string());
  REQUIRE_MESSAGE(loaded.ok, loaded.error);
  CHECK(loaded.scene.meshes.size() == 1);
  CHECK(loaded.scene.skins.empty());
  CHECK(loaded.scene.find_node("Floor") != kNoIndex);
  std::filesystem::remove(file);
}

TEST_CASE("redoing the import after a save brings the whole model back into the file") {
  const std::filesystem::path file = temp_scene("redone_import");
  SceneDocument doc = saved_floor(file);
  std::string error;
  REQUIRE_MESSAGE(doc.import_model(fixture("skinned.glb"), error) != kNoIndex, error);
  REQUIRE_MESSAGE(doc.save(error), error);
  const LoadResult with_model = load_gltf_scene(file.string());
  REQUIRE_MESSAGE(with_model.ok, with_model.error);
  REQUIRE(doc.undo());
  REQUIRE_MESSAGE(doc.save(error), error);
  REQUIRE(doc.redo());
  REQUIRE_MESSAGE(doc.save(error), error);
  const LoadResult again = load_gltf_scene(file.string());
  REQUIRE_MESSAGE(again.ok, again.error);
  REQUIRE(again.scene.meshes.size() == with_model.scene.meshes.size());
  CHECK(again.scene.skins.size() == with_model.scene.skins.size());
  CHECK(again.scene.animations.size() == with_model.scene.animations.size());
  for (std::size_t i = 0; i < again.scene.meshes.size(); ++i) {
    CAPTURE(i);
    CHECK(again.scene.meshes[i].primitives.front().positions ==
          with_model.scene.meshes[i].primitives.front().positions);
    CHECK(again.scene.meshes[i].primitives.front().indices == with_model.scene.meshes[i].primitives.front().indices);
  }
  std::filesystem::remove(file);
}

TEST_CASE("a deleted object's mesh leaves the file, and the others keep theirs") {
  const std::filesystem::path file = temp_scene("deleted_object");
  SceneDocument doc = saved_floor(file);
  std::string error;
  const GlbFile floor_only = *read_glb(file, error);
  const LoadResult floor = load_gltf_scene(file.string());
  REQUIRE_MESSAGE(floor.ok, floor.error);
  const int model = doc.import_model(fixture("hierarchy.glb"), error);
  REQUIRE_MESSAGE(model != kNoIndex, error);
  REQUIRE_MESSAGE(doc.save(error), error);
  CHECK(read_glb(file, error)->bin.size() > floor_only.bin.size());
  doc.remove(model);
  REQUIRE_MESSAGE(doc.save(error), error);
  // (The names of the extensions the model used stay listed: a few bytes, and no data.)
  const std::optional<GlbFile> after = read_glb(file, error);
  REQUIRE_MESSAGE(after.has_value(), error);
  CHECK(after->bin.size() == floor_only.bin.size());
  for (const char* list : {"meshes", "materials", "accessors", "bufferViews"}) {
    CAPTURE(list);
    CHECK(after->json[list].size() == floor_only.json[list].size());
  }
  CHECK_FALSE(after->json.contains("textures"));
  const LoadResult loaded = load_gltf_scene(file.string());
  REQUIRE_MESSAGE(loaded.ok, loaded.error);
  REQUIRE(loaded.scene.meshes.size() == 1);
  CHECK(loaded.scene.meshes.front().primitives.front().positions ==
        floor.scene.meshes.front().primitives.front().positions);
  // Undoing the deletion puts the model back for the next save.
  REQUIRE(doc.undo());
  REQUIRE_MESSAGE(doc.save(error), error);
  const LoadResult restored = load_gltf_scene(file.string());
  REQUIRE_MESSAGE(restored.ok, restored.error);
  CHECK(restored.scene.find_node("Child") != kNoIndex);
  CHECK(restored.scene.meshes.size() > 1);
  std::filesystem::remove(file);
}

TEST_CASE("a scene saved as it was opened keeps everything its objects use") {
  // (The last two are the demo games' own scenes, with textures.)
  for (const char* scene : {"games/sandbox/scene.glb", "skinned.glb", "hierarchy.glb",
                            "../../games/platformer/scene.glb", "../../games/city/scene.glb"}) {
    CAPTURE(scene);
    const LoadResult before = load_gltf_scene(fixture(scene));
    REQUIRE_MESSAGE(before.ok, before.error);
    auto opened = SceneDocument::open(fixture(scene));
    REQUIRE(std::holds_alternative<SceneDocument>(opened));
    const std::filesystem::path file = temp_scene("unchanged");
    std::string error;
    REQUIRE_MESSAGE(std::get<SceneDocument>(opened).save_as(file, error), error);
    const LoadResult after = load_gltf_scene(file.string());
    REQUIRE_MESSAGE(after.ok, after.error);
    REQUIRE(after.scene.nodes.size() == before.scene.nodes.size());
    CHECK(after.scene.skins.size() == before.scene.skins.size());
    CHECK(after.scene.animations.size() == before.scene.animations.size());
    CHECK(after.scene.images.size() <= before.scene.images.size());
    for (std::size_t i = 0; i < after.scene.nodes.size(); ++i) {
      const NodeDesc& was = before.scene.nodes[i];
      const NodeDesc& is = after.scene.nodes[i];
      CAPTURE(was.name);
      REQUIRE((is.mesh == kNoIndex) == (was.mesh == kNoIndex));
      if (was.mesh == kNoIndex) continue;
      const PrimitiveDesc& old_part = before.scene.meshes[was.mesh].primitives.front();
      const PrimitiveDesc& new_part = after.scene.meshes[is.mesh].primitives.front();
      CHECK(new_part.positions == old_part.positions);
      CHECK(new_part.indices == old_part.indices);
      CHECK((new_part.material == kNoIndex) == (old_part.material == kNoIndex));
    }
    std::filesystem::remove(file);
  }
}
