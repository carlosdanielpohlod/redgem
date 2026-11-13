// Data a scene holds twice is written once, what nothing uses is left out of the saved file, and
// neither changes what undo brings back.

#include <doctest/doctest.h>

#include <filesystem>
#include <optional>
#include <string>
#include <variant>

#include "editor/document/glb_compact.hpp"
#include "editor/document/glb_dedup.hpp"
#include "editor/document/scene_document.hpp"
#include "scene/gltf/gltf_loader.hpp"
#include "scene/property.hpp"
#include "scene/props.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::fixture;

namespace {

constexpr Color kGray{0.5f, 0.5f, 0.5f, 1.0f};
constexpr const char* kPicture = "images/sniff.png";
constexpr const char* kModelChild = "Child";       // an object of hierarchy.glb, with a mesh
constexpr const char* kSecondChild = "Child.001";  // the same object of a second import
constexpr const char* kPrefabFile = "prefabs/crate.glb";
// The bytes the hand-made file below packs once its repeats are merged: one vertex view of 8
// bytes and one picture of 3.
constexpr std::size_t kVertexBytes = 8;
constexpr std::size_t kPictureBytes = 3;

// Two objects, each with a mesh, a material, a texture, a picture and bytes of their own; but the
// bytes of both meshes are the same (0..7 and 8..15), and so are those of both pictures.
GlbFile two_copies() {
  GlbFile glb;
  glb.bin = {1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8, 9, 9, 9, 0, 9, 9, 9, 0};
  glb.json = nlohmann::json::parse(R"({
    "asset": {"version": "2.0"},
    "buffers": [{"byteLength": 24}],
    "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": 8}, {"buffer": 0, "byteOffset": 8, "byteLength": 8},
                    {"buffer": 0, "byteOffset": 16, "byteLength": 3}, {"buffer": 0, "byteOffset": 20, "byteLength": 3}],
    "accessors": [{"bufferView": 0, "count": 2, "componentType": 5126, "type": "SCALAR"},
                  {"bufferView": 1, "count": 2, "componentType": 5126, "type": "SCALAR"}],
    "images": [{"bufferView": 2, "mimeType": "image/png"}, {"bufferView": 3, "mimeType": "image/png"}],
    "textures": [{"source": 0}, {"source": 1}],
    "materials": [{"name": "A", "pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}},
                  {"name": "B", "pbrMetallicRoughness": {"baseColorTexture": {"index": 1}}}],
    "meshes": [{"name": "A", "primitives": [{"attributes": {"POSITION": 0}, "material": 0}]},
               {"name": "B", "primitives": [{"attributes": {"POSITION": 1}, "material": 1}]}],
    "nodes": [{"name": "A", "mesh": 0}, {"name": "B", "mesh": 1}],
    "scenes": [{"nodes": [0, 1]}],
    "scene": 0
  })");
  return glb;
}

int texture_of(const nlohmann::json& material) {
  return material["pbrMetallicRoughness"]["baseColorTexture"]["index"].get<int>();
}

int position_of(const nlohmann::json& mesh) { return mesh["primitives"][0]["attributes"]["POSITION"].get<int>(); }

std::filesystem::path temp_scene(const std::string& name) {
  return std::filesystem::temp_directory_path() / ("redgem_dedup_" + name + ".glb");
}

GlbFile read_scene(const std::filesystem::path& file) {
  std::string error;
  std::optional<GlbFile> glb = read_glb(file, error);
  REQUIRE_MESSAGE(glb.has_value(), error);
  return std::move(*glb);
}

SceneDesc load(const std::filesystem::path& file) {
  LoadResult loaded = load_gltf_scene(file.string());
  REQUIRE_MESSAGE(loaded.ok, loaded.error);
  return std::move(loaded.scene);
}

// hierarchy.glb with a picture on its Child's material, saved as a model of its own to import.
std::filesystem::path textured_model() {
  auto opened = SceneDocument::open(fixture("hierarchy.glb"));
  REQUIRE(std::holds_alternative<SceneDocument>(opened));
  SceneDocument& model = std::get<SceneDocument>(opened);
  std::string error;
  const int image = model.add_image(fixture(kPicture), error);
  REQUIRE_MESSAGE(image != kNoIndex, error);
  const int material = model.material_of(model.desc().find_node(kModelChild));
  MaterialDesc edited = model.material(material);
  edited.base_color_image = image;
  model.set_material(material, edited);
  const std::filesystem::path file = temp_scene("textured_model");
  REQUIRE_MESSAGE(model.save_as(file, error), error);
  return file;
}

// A scene with a floor, saved.
SceneDocument saved_floor(const std::filesystem::path& file) {
  SceneDocument doc = SceneDocument::create_empty(file);
  doc.add_primitive(PrimitiveShape::Plane, "Floor", kGray);
  std::string error;
  REQUIRE_MESSAGE(doc.save(error), error);
  return doc;
}

int import_into(SceneDocument& doc, const std::filesystem::path& model) {
  std::string error;
  const int root = doc.import_model(model, error);
  REQUIRE_MESSAGE(root != kNoIndex, error);
  return root;
}

void save_scene(SceneDocument& doc) {
  std::string error;
  REQUIRE_MESSAGE(doc.save(error), error);
}

const PrimitiveDesc& part_of(const SceneDesc& scene, const std::string& object) {
  const int node = scene.find_node(object);
  REQUIRE(node != kNoIndex);
  REQUIRE(scene.nodes[node].mesh != kNoIndex);
  return scene.meshes[scene.nodes[node].mesh].primitives.front();
}

// The picture the object of that name shows (its material's color map).
const std::vector<std::uint8_t>& picture_of(const SceneDesc& scene, const std::string& object) {
  const int material = part_of(scene, object).material;
  REQUIRE(material != kNoIndex);
  const int image = scene.materials[material].base_color_image;
  REQUIRE(image != kNoIndex);
  return scene.images[image].encoded;
}

// The object of that name in `scene` looks as the model's Child does: same vertices, same picture.
void check_like_model(const SceneDesc& scene, const std::string& object, const SceneDesc& model) {
  CAPTURE(object);
  CHECK(part_of(scene, object).positions == part_of(model, kModelChild).positions);
  CHECK(part_of(scene, object).indices == part_of(model, kModelChild).indices);
  CHECK(picture_of(scene, object) == picture_of(model, kModelChild));
}

std::size_t entries(const GlbFile& glb, const char* list) {
  return glb.json.contains(list) ? glb.json[list].size() : 0;
}

}  // namespace

TEST_CASE("repeated bytes, pictures and textures point at their first copy; materials and meshes stay apart") {
  const GlbFile glb = two_copies();
  const std::optional<nlohmann::json> merged = with_repeats_merged(glb.json, glb.bin);
  REQUIRE(merged.has_value());
  CHECK((*merged)["accessors"][1]["bufferView"] == 0);
  CHECK((*merged)["images"][1]["bufferView"] == 2);
  CHECK(position_of((*merged)["meshes"][1]) == 0);
  CHECK((*merged)["textures"][1]["source"] == 0);
  CHECK(texture_of((*merged)["materials"][1]) == 0);
  CHECK((*merged)["materials"][1]["name"] == "B");
  CHECK((*merged)["nodes"][1]["mesh"] == 1);
  CHECK(merged->size() == glb.json.size());  // the repeats are still there, pointed at by nothing
}

TEST_CASE("a compacted file keeps one copy of each repeat, numbered anew, and every reference follows") {
  const std::optional<GlbFile> lean = compacted(two_copies());
  REQUIRE(lean.has_value());
  CHECK(lean->bin.size() == glb_padded_size(kVertexBytes) + kPictureBytes);
  CHECK(entries(*lean, "bufferViews") == 2);
  CHECK(entries(*lean, "accessors") == 1);
  CHECK(entries(*lean, "images") == 1);
  CHECK(entries(*lean, "textures") == 1);
  REQUIRE(entries(*lean, "materials") == 2);
  REQUIRE(entries(*lean, "meshes") == 2);
  CHECK(lean->json["images"][0]["bufferView"] == 1);  // the picture's bytes now come second
  CHECK(lean->json["bufferViews"][1]["byteOffset"] == glb_padded_size(kVertexBytes));
  for (const nlohmann::json& mesh : lean->json["meshes"]) CHECK(position_of(mesh) == 0);
  for (const nlohmann::json& material : lean->json["materials"]) CHECK(texture_of(material) == 0);
  CHECK(lean->json["meshes"][1]["primitives"][0]["material"] == 1);
}

TEST_CASE("bytes that differ are not merged, and a file with nothing repeated is left as it is") {
  GlbFile different = two_copies();
  different.bin[kVertexBytes] = 0;  // the second mesh's first byte
  const std::optional<nlohmann::json> merged = with_repeats_merged(different.json, different.bin);
  REQUIRE(merged.has_value());  // the pictures still are the same
  CHECK((*merged)["accessors"][1]["bufferView"] == 1);
  CHECK((*merged)["textures"][1]["source"] == 0);
  const std::optional<GlbFile> lean = compacted(different);
  REQUIRE(lean.has_value());
  CHECK_FALSE(with_repeats_merged(lean->json, lean->bin).has_value());
}

TEST_CASE("a terrain's vertices are neither merged into another mesh's nor shared with one") {
  GlbFile glb = two_copies();
  glb.json["nodes"][1]["extras"] = {{props::kTerrain, true}};
  const std::optional<GlbFile> lean = compacted(glb);
  REQUIRE(lean.has_value());
  CHECK(entries(*lean, "accessors") == 2);
  CHECK(position_of(lean->json["meshes"][0]) != position_of(lean->json["meshes"][1]));
  CHECK(entries(*lean, "images") == 1);  // its pictures are merged like any other
  glb.json["nodes"][0]["extras"] = {{props::kTerrain, true}};
  glb.json["nodes"][1].erase("extras");
  CHECK(entries(*compacted(glb), "accessors") == 2);  // whichever of the two comes first
}

TEST_CASE("a file pointing at its data in a way not followed is not merged") {
  GlbFile glb = two_copies();
  glb.json["bufferViews"][1]["extensions"] = {{"EXT_meshopt_compression", nlohmann::json::object()}};
  CHECK_FALSE(with_repeats_merged(glb.json, glb.bin).has_value());
  GlbFile past_the_end = two_copies();
  past_the_end.json["bufferViews"][3]["byteLength"] = past_the_end.bin.size();
  CHECK_FALSE(with_repeats_merged(past_the_end.json, past_the_end.bin).has_value());
}

TEST_CASE("importing the same model twice adds its data to the saved scene once") {
  const std::filesystem::path model = textured_model();
  const std::filesystem::path file = temp_scene("imported_twice");
  SceneDocument doc = saved_floor(file);
  import_into(doc, model);
  save_scene(doc);
  const GlbFile once = read_scene(file);
  import_into(doc, model);
  save_scene(doc);
  const GlbFile twice = read_scene(file);
  CHECK(twice.bin.size() == once.bin.size());
  for (const char* list : {"bufferViews", "accessors", "images", "textures"}) {
    CAPTURE(list);
    CHECK(entries(twice, list) == entries(once, list));
  }
  CHECK(entries(twice, "meshes") > entries(once, "meshes"));  // each object keeps a mesh to edit
  CHECK(doc.last_save().left_out_bytes > 0);
  CHECK(doc.last_save().written_bytes == twice.bin.size());
  const SceneDesc saved = load(file);
  const SceneDesc original = load(model);
  check_like_model(saved, kModelChild, original);
  check_like_model(saved, kSecondChild, original);
  std::filesystem::remove(file);
  std::filesystem::remove(model);
}

TEST_CASE("deleting an imported copy and saving drops its data; undo after the save brings it back whole") {
  const std::filesystem::path model = textured_model();
  const std::filesystem::path file = temp_scene("deleted_copy");
  SceneDocument doc = saved_floor(file);
  const GlbFile floor_only = read_scene(file);
  const int first = import_into(doc, model);
  import_into(doc, model);
  save_scene(doc);
  const GlbFile both = read_scene(file);
  doc.remove(first);  // the copy whose data the file kept
  save_scene(doc);
  const GlbFile second_only = read_scene(file);
  CHECK(second_only.bin.size() == both.bin.size());
  CHECK(entries(second_only, "meshes") < entries(both, "meshes"));
  const SceneDesc original = load(model);
  check_like_model(load(file), kSecondChild, original);
  CHECK(load(file).find_node(kModelChild) == kNoIndex);
  REQUIRE(doc.undo());
  save_scene(doc);
  check_like_model(load(file), kModelChild, original);
  check_like_model(load(file), kSecondChild, original);
  REQUIRE(doc.undo());  // and the second import too: nothing of the model is left
  REQUIRE(doc.undo());
  save_scene(doc);
  CHECK(read_scene(file).bin.size() == floor_only.bin.size());
  std::filesystem::remove(file);
  std::filesystem::remove(model);
}

TEST_CASE("a scene saved, opened and saved again keeps everything its objects use") {
  const std::filesystem::path model = textured_model();
  const std::filesystem::path file = temp_scene("round_trip");
  SceneDocument doc = saved_floor(file);
  import_into(doc, model);
  import_into(doc, model);
  save_scene(doc);
  const GlbFile first = read_scene(file);
  auto reopened = SceneDocument::open(file);
  REQUIRE(std::holds_alternative<SceneDocument>(reopened));
  save_scene(std::get<SceneDocument>(reopened));
  CHECK(std::get<SceneDocument>(reopened).last_save().left_out_bytes == 0);
  const GlbFile again = read_scene(file);
  CHECK(again.bin == first.bin);
  for (const char* list : {"nodes", "meshes", "materials", "accessors", "bufferViews", "images", "textures"}) {
    CAPTURE(list);
    CHECK(entries(again, list) == entries(first, list));
  }
  const SceneDesc original = load(model);
  check_like_model(load(file), kModelChild, original);
  check_like_model(load(file), kSecondChild, original);
  std::filesystem::remove(file);
  std::filesystem::remove(model);
}

TEST_CASE("templates and prefab placements keep what they use; the save logs what it left out") {
  const std::filesystem::path model = textured_model();
  const std::filesystem::path file = temp_scene("prefabs");
  SceneDocument doc = saved_floor(file);
  const int template_root = import_into(doc, model);
  doc.set_prop(template_root, props::kPrefab, true);  // spawned by scripts, never placed by hand
  const int placement = doc.add_empty("Crate");
  doc.set_prop(placement, props::kPrefabFile, std::string(kPrefabFile));
  doc.remove(import_into(doc, model));
  test::LogCapture log;
  save_scene(doc);
  CHECK(log.contains("left out"));
  const SceneDesc saved = load(file);
  check_like_model(saved, kModelChild, load(model));
  const int crate = saved.find_node("Crate");
  REQUIRE(crate != kNoIndex);
  CHECK(get_string(saved.nodes[crate].props, props::kPrefabFile).value_or("") == kPrefabFile);
  std::filesystem::remove(file);
  std::filesystem::remove(model);
}
