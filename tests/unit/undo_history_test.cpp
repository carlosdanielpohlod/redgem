#include <doctest/doctest.h>

#include <filesystem>
#include <string>

#include "editor/document/scene_document.hpp"
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
  // And the scene saves with it.
  REQUIRE_MESSAGE(doc.save(error), error);
  std::filesystem::remove(doc.path());
}
