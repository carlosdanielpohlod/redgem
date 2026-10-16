#include <doctest/doctest.h>

#include <cstdint>
#include <filesystem>
#include <iterator>
#include <string>

#include <nlohmann/json.hpp>

#include "editor/document/glb_file.hpp"
#include "editor/document/scene_document.hpp"
#include "scene/gltf/gltf_loader.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::fixture;

namespace {

constexpr std::size_t kStepsPastTheLimit = 10;
// Terrains as big as a mesh can be: every undo step keeps a copy of each one's shape (1.5 MB).
constexpr TerrainGrid kBigGrid{100.0f, 254};
constexpr int kBigTerrains = 8;
constexpr std::size_t kRenamesPastTheBytes = 30;  // more steps than the bytes allow with those terrains
constexpr Color kGrass{0.3f, 0.5f, 0.2f, 1.0f};
// A model imported and deleted again and again: more times than undo keeps steps for.
constexpr int kImportCycles = static_cast<int>(SceneDocument::kMaxUndoSteps);
constexpr int kCyclesUndoReaches = static_cast<int>(SceneDocument::kMaxUndoSteps) / 2;  // two steps each

// A model of one triangle painted with a big picture: the picture is what an import of it costs.
constexpr std::size_t kPictureBytes = 48 * 1024 * 1024;
constexpr int kPaintedCycles = 6;  // imported and deleted: more than undo has bytes to keep
constexpr std::uint8_t kPngStart[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n', 0, 0, 0, 13, 'I', 'H', 'D', 'R',
                                      0,    0,   0,   64,  0,    0,    0,    64,   8, 6, 0, 0,  0,   0,   0,   0};
constexpr float kTriangle[] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
constexpr std::uint16_t kCorners[] = {0, 1, 2, 0};  // (the last one pads the indices to four bytes)

std::filesystem::path painted_model() {
  GlbFile glb;
  const auto* positions = reinterpret_cast<const std::uint8_t*>(kTriangle);
  const auto* indices = reinterpret_cast<const std::uint8_t*>(kCorners);
  glb.bin.insert(glb.bin.end(), positions, positions + sizeof(kTriangle));
  glb.bin.insert(glb.bin.end(), indices, indices + sizeof(kCorners));
  const std::size_t picture_at = glb.bin.size();
  glb.bin.insert(glb.bin.end(), std::begin(kPngStart), std::end(kPngStart));
  glb.bin.resize(picture_at + kPictureBytes);
  glb.json = nlohmann::json::parse(R"({
    "asset": {"version": "2.0"}, "scene": 0, "scenes": [{"nodes": [0]}],
    "nodes": [{"name": "Painted", "mesh": 0}],
    "meshes": [{"primitives": [{"attributes": {"POSITION": 0}, "indices": 1, "material": 0}]}],
    "materials": [{"pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}}],
    "textures": [{"source": 0}],
    "images": [{"bufferView": 2, "mimeType": "image/png"}],
    "accessors": [{"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3", "min": [0, 0, 0], "max": [1, 1, 0]},
                  {"bufferView": 1, "componentType": 5123, "count": 3, "type": "SCALAR"}]
  })");
  glb.json["bufferViews"] = {
      {{"buffer", 0}, {"byteOffset", 0}, {"byteLength", sizeof(kTriangle)}},
      {{"buffer", 0}, {"byteOffset", sizeof(kTriangle)}, {"byteLength", 3 * sizeof(std::uint16_t)}},
      {{"buffer", 0}, {"byteOffset", picture_at}, {"byteLength", kPictureBytes}}};
  glb.json["buffers"] = {{{"byteLength", glb.bin.size()}}};
  const std::filesystem::path file = std::filesystem::temp_directory_path() / "redgem_undo_painted.glb";
  std::string error;
  REQUIRE_MESSAGE(write_glb(file, glb, error), error);
  return file;
}

SceneDocument empty_document(const std::string& name) {
  return SceneDocument::create_empty(std::filesystem::temp_directory_path() / ("redgem_undo_" + name + ".glb"));
}

// How many times the document can be undone (it is left undone all the way).
std::size_t undo_all(SceneDocument& doc) {
  std::size_t steps = 0;
  while (doc.undo()) ++steps;
  return steps;
}

std::size_t vertex_floats(const SceneDesc& desc) {
  std::size_t floats = 0;
  for (const MeshDesc& mesh : desc.meshes) {
    for (const PrimitiveDesc& part : mesh.primitives) floats += part.positions.size();
  }
  return floats;
}

// The vertices of the object called "Child" (the model's), as a scene holds them.
std::vector<float> child_positions(const SceneDesc& scene) {
  const int child = scene.find_node("Child");
  REQUIRE(child != kNoIndex);
  const int mesh = scene.nodes.at(static_cast<std::size_t>(child)).mesh;
  return scene.meshes.at(static_cast<std::size_t>(mesh)).primitives.at(0).positions;
}

}  // namespace

TEST_CASE("undo keeps its latest steps up to its limit, and each of them still works") {
  SceneDocument doc = empty_document("steps");
  const int thing = doc.add_empty("Thing");
  const std::size_t renames = SceneDocument::kMaxUndoSteps + kStepsPastTheLimit;
  for (std::size_t i = 0; i < renames; ++i) doc.set_name(thing, "Name" + std::to_string(i));
  REQUIRE(doc.undo());
  CHECK(doc.node(thing).name == "Name" + std::to_string(renames - 2));
  REQUIRE(doc.redo());
  CHECK(undo_all(doc) == SceneDocument::kMaxUndoSteps);
  // The oldest steps went: as far back as undo goes, the object is already there and renamed.
  CHECK(doc.node(thing).name == "Name" + std::to_string(kStepsPastTheLimit - 1));
}

TEST_CASE("undo keeps no more bytes than its limit, dropping its oldest steps") {
  SceneDocument doc = empty_document("bytes");
  for (int i = 0; i < kBigTerrains; ++i) doc.add_terrain("Hills", kBigGrid, kGrass);
  const int thing = doc.add_empty("Thing");
  for (std::size_t i = 0; i < kRenamesPastTheBytes; ++i) {
    doc.set_name(thing, "Name" + std::to_string(i));
    CHECK(doc.history_bytes() <= SceneDocument::kMaxHistoryBytes);
  }
  // Steps went for the bytes (not for their count), and the latest ones still undo.
  REQUIRE(doc.undo());
  CHECK(doc.node(thing).name == "Name" + std::to_string(kRenamesPastTheBytes - 2));
  const std::size_t steps = undo_all(doc) + 1;
  CHECK(steps > 1);
  CHECK(steps < kRenamesPastTheBytes);
  CHECK(doc.node_count() == kBigTerrains + 1);  // the terrains were made by steps that went
}

TEST_CASE("what undo keeps is counted as its steps come and go, without walking them all") {
  constexpr int kStepsAfterTheImport = 2;  // the deletion and the rename
  constexpr int kStepsBackToTheThing = 3;  // those, and the import
  SceneDocument doc = empty_document("counted");
  const int thing = doc.add_empty("Thing");
  std::string error;
  const int model = doc.import_model(fixture("hierarchy.glb"), error);
  REQUIRE_MESSAGE(model != kNoIndex, error);
  doc.remove(model);
  doc.set_name(thing, "Renamed");
  const std::size_t kept = doc.history_bytes();
  // Undone and redone, the steps hold what they held. (Not past the import: the materials it
  // added stay, so a step from before it is copied with more of them when it is redone.)
  for (int step = 0; step < kStepsAfterTheImport; ++step) REQUIRE(doc.undo());
  for (int step = 0; step < kStepsAfterTheImport; ++step) REQUIRE(doc.redo());
  CHECK(doc.history_bytes() == kept);
  // A step that changes nothing goes again, and takes its bytes along.
  doc.set_name(thing, "Renamed");
  doc.drop_checkpoint_if_unchanged();
  CHECK(doc.history_bytes() == kept);
  // Back before the import, a new step lets go of the steps that showed the model, and of the model.
  for (int step = 0; step < kStepsBackToTheThing; ++step) REQUIRE(doc.undo());
  const std::size_t with_redo = doc.history_bytes();
  doc.set_name(thing, "Again");
  CHECK(doc.history_bytes() < with_redo);
  CHECK_FALSE(doc.can_redo());
}

TEST_CASE("models imported and deleted over and over: what no step can bring back is let go of") {
  SceneDocument doc = empty_document("imports");
  std::string error;
  std::size_t held_after_reach = 0;
  for (int cycle = 0; cycle < kImportCycles; ++cycle) {
    const int model = doc.import_model(fixture("hierarchy.glb"), error);
    REQUIRE_MESSAGE(model != kNoIndex, error);
    doc.remove(model);
    // Once undo cannot reach the first imports any more, what is held stops growing.
    if (cycle == kCyclesUndoReaches + 1) held_after_reach = vertex_floats(doc.desc());
  }
  CHECK(held_after_reach > 0);
  CHECK(vertex_floats(doc.desc()) <= held_after_reach);
  CHECK(doc.desc().meshes.front().primitives.empty());  // the first import's
  // Undo still brings the last model back, with its vertices.
  REQUIRE(doc.undo());
  const int child = doc.desc().find_node("Child");
  REQUIRE(child != kNoIndex);
  CHECK_FALSE(doc.desc().meshes.at(static_cast<std::size_t>(doc.node(child).mesh)).primitives.empty());
  // And the scene saves with it, and with its own vertices: the imports' bytes that moved down the
  // file as older ones were let go of (on the imports' own steps too) are still theirs.
  REQUIRE_MESSAGE(doc.save(error), error);
  const LoadResult saved = load_gltf_scene(doc.path().string());
  const LoadResult original = load_gltf_scene(fixture("hierarchy.glb"));
  REQUIRE_MESSAGE(saved.ok, saved.error);
  REQUIRE_MESSAGE(original.ok, original.error);
  CHECK(child_positions(saved.scene) == child_positions(original.scene));
  std::filesystem::remove(doc.path());
}

TEST_CASE("the pictures of deleted imports count in what undo keeps, which stays within its limit") {
  const std::filesystem::path model = painted_model();
  SceneDocument doc = empty_document("pictures");
  const int keeper = doc.add_empty("Keeper");
  std::string error;
  // One import, deleted: undo keeps its picture twice (the scene's copy, and its bytes in the file).
  int painted = doc.import_model(model, error);
  REQUIRE_MESSAGE(painted != kNoIndex, error);
  doc.remove(painted);
  doc.set_name(keeper, "Keeper after the first");
  CHECK(doc.history_bytes() >= 2 * kPictureBytes);
  for (int cycle = 0; cycle < kPaintedCycles; ++cycle) {
    painted = doc.import_model(model, error);
    REQUIRE_MESSAGE(painted != kNoIndex, error);
    // (Checked as each step began: the latest one, the deletion below, may take undo past it.)
    CHECK(doc.history_bytes() <= SceneDocument::kMaxHistoryBytes);
    doc.remove(painted);
  }
  doc.set_name(keeper, "Keeper at the end");
  CHECK(doc.history_bytes() <= SceneDocument::kMaxHistoryBytes);
  // What undo still reaches works: the last model comes back with its picture.
  REQUIRE(doc.undo());
  REQUIRE(doc.undo());
  const int back = doc.desc().find_node("Painted");
  REQUIRE(back != kNoIndex);
  const int material = doc.material_of(back);
  REQUIRE(material != kNoIndex);
  const int image = doc.material(material).base_color_image;
  REQUIRE(image != kNoIndex);
  CHECK(doc.desc().images.at(static_cast<std::size_t>(image)).encoded.size() == kPictureBytes);
  std::filesystem::remove(model);
}
