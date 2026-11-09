#include <doctest/doctest.h>

#include <filesystem>
#include <string>

#include "core/files.hpp"
#include "editor/editor_session.hpp"
#include "editor/scene_recovery.hpp"
#include "scene/props.hpp"

using namespace redgem;
namespace {
namespace fs = std::filesystem;
struct RecoveryGame {
  fs::path folder = fs::temp_directory_path() / "redgem_scene_recovery_test";
  fs::path source = folder / "scene.glb";
  std::string error;
  SceneDocument document = SceneDocument::create_empty(source);
  RecoveryGame() {
    fs::remove_all(folder);
    fs::create_directories(folder);
    document.add_primitive(PrimitiveShape::Cube, "Original", {1, 0, 0, 1});
    REQUIRE_MESSAGE(document.save(error), error);
  }
  ~RecoveryGame() { fs::remove_all(folder); }
  std::string bytes(const fs::path& path) {
    auto result = read_text_file(path, error);
    REQUIRE_MESSAGE(result.has_value(), error);
    return *result;
  }
  void autosave(SceneRecovery& recovery) {
    REQUIRE_MESSAGE(recovery.poll(document, SceneRecovery::kIntervalSeconds, error), error);
  }
};
}  // namespace
TEST_CASE("scene recovery writes a separate authored copy and preserves undo state") {
  RecoveryGame game;
  SceneRecovery recovery;
  recovery.watch(game.source);
  const auto original = game.bytes(game.source);
  const auto id = game.document.node(0).stable_id;
  game.document.set_name(0, "Edited");
  const auto version = game.document.version();
  game.autosave(recovery);
  CHECK(game.document.path() == game.source);
  CHECK(game.document.version() == version);
  CHECK(game.document.dirty());
  CHECK(game.bytes(game.source) == original);
  CHECK(game.document.can_undo());
  game.document.undo();
  CHECK(game.document.node(0).name == "Original");
  CHECK_FALSE(game.document.dirty());
  game.document.redo();
  CHECK(game.document.node(0).name == "Edited");
  recovery.stop();
  SceneRecovery restarted;
  restarted.watch(game.source);
  REQUIRE(restarted.pending());
  CHECK_FALSE(restarted.source_changed());
  REQUIRE_MESSAGE(restarted.recover(game.document, game.error), game.error);
  CHECK(game.document.node(0).stable_id == id);
  CHECK(game.document.node(0).name == "Edited");
  CHECK(game.document.dirty());
  CHECK(game.document.path() == game.source);
  CHECK(game.bytes(game.source) == original);
  REQUIRE(game.document.save(game.error));
  REQUIRE(restarted.discard(game.error));
  CHECK_FALSE(fs::exists(SceneRecovery::copy_path(game.source)));
}
TEST_CASE("scene recovery coalesces versions and clean undo removes the old backup") {
  RecoveryGame game;
  SceneRecovery recovery;
  recovery.watch(game.source);
  game.document.set_name(0, "Edited");
  REQUIRE(recovery.poll(game.document, SceneRecovery::kIntervalSeconds - 1, game.error));
  const auto copy = SceneRecovery::copy_path(game.source);
  CHECK_FALSE(fs::exists(copy));
  REQUIRE(recovery.poll(game.document, 1, game.error));
  const auto time = fs::last_write_time(copy);
  const auto bytes = game.bytes(copy);
  game.autosave(recovery);
  CHECK(fs::last_write_time(copy) == time);
  CHECK(game.bytes(copy) == bytes);
  game.document.undo();
  game.autosave(recovery);
  CHECK_FALSE(fs::exists(copy));
}
TEST_CASE("scene recovery refuses damaged and misplaced copies without altering the document") {
  RecoveryGame game;
  SceneRecovery recovery;
  recovery.watch(game.source);
  game.document.set_name(0, "Edited");
  game.autosave(recovery);
  const auto copy = SceneRecovery::copy_path(game.source);
  const auto good = game.bytes(copy);
  REQUIRE(write_text_file(copy, good.substr(0, good.size() / 2), game.error));
  recovery.watch(game.source);
  CHECK(recovery.source_changed());
  CHECK_FALSE(recovery.recover(game.document, game.error));
  CHECK(game.document.node(0).name == "Edited");
  REQUIRE(write_text_file(copy, good, game.error));
  auto glb = read_glb(copy, game.error);
  REQUIRE(glb);
  glb->json["redgem_editor_recovery"]["source"] = "other.glb";
  REQUIRE(write_glb(copy, *glb, game.error));
  CHECK_FALSE(recovery.recover(game.document, game.error));
  CHECK(game.document.node(0).name == "Edited");
}
TEST_CASE("scene recovery keeps a valid backup when flushing its replacement fails") {
  RecoveryGame game;
  bool fail_flush = false;
  SceneRecovery recovery([&](std::FILE* file) { return !fail_flush && durable::flush_to_disk(file); });
  recovery.watch(game.source);
  game.document.set_name(0, "First edit");
  game.autosave(recovery);
  const auto copy = SceneRecovery::copy_path(game.source);
  const auto good = game.bytes(copy);
  const auto original = game.bytes(game.source);
  fail_flush = true;
  game.document.set_name(0, "Second edit");
  CHECK_FALSE(recovery.poll(game.document, SceneRecovery::kIntervalSeconds, game.error));
  CHECK(game.bytes(copy) == good);
  CHECK(game.bytes(game.source) == original);
  CHECK(game.document.dirty());
  for (const auto& entry : fs::directory_iterator(game.folder)) {
    auto lease_file = copy;
    lease_file += ".lock";
    const bool expected_file = entry.path() == game.source || entry.path() == copy || entry.path() == lease_file;
    CHECK(expected_file);
  }
  fail_flush = false;
  game.document.set_name(0, "Third edit");
  game.autosave(recovery);
  CHECK(game.bytes(copy) != good);
}
TEST_CASE("scene recovery reports a source conflict even when size and timestamp match") {
  RecoveryGame game;
  SceneRecovery recovery;
  recovery.watch(game.source);
  game.document.set_name(0, "Recovered");
  game.autosave(recovery);
  const auto time = fs::last_write_time(game.source);
  const auto size = fs::file_size(game.source);
  auto opened = SceneDocument::open(game.source);
  REQUIRE(std::holds_alternative<SceneDocument>(opened));
  auto& other = std::get<SceneDocument>(opened);
  other.set_name(0, "External");
  REQUIRE(other.save(game.error));
  REQUIRE(fs::file_size(game.source) == size);
  fs::last_write_time(game.source, time);
  const auto external = game.bytes(game.source);
  recovery.watch(game.source);
  REQUIRE(recovery.source_changed());
  REQUIRE(recovery.recover(game.document, game.error));
  CHECK(game.document.node(0).name == "Recovered");
  CHECK(game.bytes(game.source) == external);
}
TEST_CASE("scene recovery preserves terrain edits and raw prefab references") {
  RecoveryGame game;
  const int ground = game.document.add_terrain("Ground", TerrainGrid{8, 4}, {0, 1, 0, 1});
  const int instance = game.document.add_empty("Linked");
  game.document.set_prop(instance, props::kPrefabFile, std::string("prefabs/shape.glb"));
  REQUIRE(game.document.save(game.error));
  SceneRecovery recovery;
  recovery.watch(game.source);
  game.document.checkpoint();
  REQUIRE(game.document.sculpt_terrain(ground, TerrainBrush{0, 0, 6, 3, SculptMode::Raise}));
  const auto heights = game.document.desc().meshes.at(game.document.node(ground).mesh).primitives.front().positions;
  game.autosave(recovery);
  // Another edit must not lose terrain bytes after an earlier recovery sync.
  game.document.set_name(instance, "Placed");
  game.autosave(recovery);
  recovery.watch(game.source);
  REQUIRE(recovery.recover(game.document, game.error));
  CHECK(game.document.desc().meshes.at(game.document.node(ground).mesh).primitives.front().positions == heights);
  CHECK(std::get<std::string>(game.document.node(instance).props.at(props::kPrefabFile)) == "prefabs/shape.glb");
  CHECK(game.document.node(instance).children.empty());
}
TEST_CASE("editor session recovers after a crash and successful Save cleans its backup") {
  RecoveryGame game;
  auto opened = EditorSession::open(game.folder);
  REQUIRE(std::holds_alternative<std::unique_ptr<EditorSession>>(opened));
  auto session = std::move(std::get<std::unique_ptr<EditorSession>>(opened));
  session->document().set_name(0, "Unsaved");
  session->autosave(SceneRecovery::kIntervalSeconds);
  session.reset();
  opened = EditorSession::open(game.folder);
  session = std::move(std::get<std::unique_ptr<EditorSession>>(opened));
  REQUIRE(session->recovery_pending());
  REQUIRE(session->recover_scene(game.error));
  CHECK(session->document().node(0).name == "Unsaved");
  CHECK(session->document().dirty());
  REQUIRE(session->save_as("recovered", game.error));
  CHECK_FALSE(fs::exists(SceneRecovery::copy_path(game.source)));
  CHECK_FALSE(session->recovery_pending());
  CHECK_FALSE(session->document().dirty());
  CHECK(game.document.node(0).name == "Original");
}
TEST_CASE("scene recovery keeps linked images beside their original base directory") {
  RecoveryGame game;
  auto glb = read_glb(game.source, game.error);
  REQUIRE(glb);
  glb->json["images"] = nlohmann::json::array({{{"uri", "wood.png"}}});
  glb->json["textures"] = nlohmann::json::array({{{"source", 0}}});
  glb->json["materials"][0]["pbrMetallicRoughness"]["baseColorTexture"] = {{"index", 0}};
  REQUIRE(write_glb(game.source, *glb, game.error));
  REQUIRE(write_text_file(game.folder / "wood.png", "not an actual image", game.error));
  auto opened = SceneDocument::open(game.source);
  REQUIRE(std::holds_alternative<SceneDocument>(opened));
  game.document = std::move(std::get<SceneDocument>(opened));
  SceneRecovery recovery;
  recovery.watch(game.source);
  game.document.set_name(0, "Edited");
  game.autosave(recovery);
  recovery.watch(game.source);
  REQUIRE_MESSAGE(recovery.recover(game.document, game.error), game.error);
  CHECK(game.document.linked_files() == std::vector<std::string>{"wood.png"});
  CHECK(game.document.path().parent_path() == game.folder);
}
TEST_CASE("scene recovery never overwrites an undecided copy on reopening") {
  RecoveryGame game;
  SceneRecovery recovery;
  recovery.watch(game.source);
  game.document.set_name(0, "Recoverable");
  game.autosave(recovery);
  const auto copy = SceneRecovery::copy_path(game.source);
  const auto good = game.bytes(copy);
  recovery.watch(game.source);
  game.document.set_name(0, "Not decided");
  game.autosave(recovery);
  CHECK(game.bytes(copy) == good);
  REQUIRE(recovery.discard(game.error));
  CHECK_FALSE(recovery.pending());
  CHECK_FALSE(fs::exists(copy));
}
TEST_CASE("scene recovery rejects oversized copies before parsing or replacing the document") {
  RecoveryGame game;
  SceneRecovery recovery;
  const auto copy = SceneRecovery::copy_path(game.source);
  REQUIRE(write_text_file(copy, "x", game.error));
  fs::resize_file(copy, 64 * 1024 * 1024 + 1);
  recovery.watch(game.source);
  CHECK(recovery.pending());
  CHECK_FALSE(recovery.recover(game.document, game.error));
  CHECK(game.document.node(0).name == "Original");
  CHECK_FALSE(game.document.dirty());
}
TEST_CASE("scene recovery refuses linked buffers before changing the document's file data") {
  RecoveryGame game;
  auto glb = read_glb(game.source, game.error);
  REQUIRE(glb);
  REQUIRE(write_bytes_file(game.folder / "geometry.bin", glb->bin, game.error));
  glb->json["buffers"][0]["uri"] = "geometry.bin";
  glb->bin.clear();
  REQUIRE(write_glb(game.source, *glb, game.error));
  const auto original = game.bytes(game.source);
  auto opened = SceneDocument::open(game.source);
  REQUIRE(std::holds_alternative<SceneDocument>(opened));
  game.document = std::move(std::get<SceneDocument>(opened));
  game.document.set_name(0, "Unsaved");
  SceneRecovery recovery;
  recovery.watch(game.source);
  CHECK_FALSE(recovery.poll(game.document, SceneRecovery::kIntervalSeconds, game.error));
  CHECK(game.document.linked_files() == std::vector<std::string>{"geometry.bin"});
  CHECK(game.bytes(game.source) == original);
  CHECK_FALSE(fs::exists(SceneRecovery::copy_path(game.source)));
  CHECK(game.document.dirty());
}

TEST_CASE("scene recovery never reads or deletes a live editor's copy") {
  RecoveryGame game;
  SceneRecovery owner;
  owner.watch(game.source);
  game.document.set_name(0, "Live unsaved edit");
  game.autosave(owner);
  const auto copy = SceneRecovery::copy_path(game.source);
  const auto bytes = game.bytes(copy);
  SceneRecovery other;
  other.watch(game.source);
  CHECK_FALSE(other.pending());
  CHECK_FALSE(other.problem().empty());
  REQUIRE(other.discard(game.error));
  CHECK(game.bytes(copy) == bytes);
  CHECK_FALSE(other.recover(game.document, game.error));
  owner.stop();  // the same release the OS performs when the writer exits
  other.watch(game.source);
  REQUIRE(other.pending());
  REQUIRE(other.recover(game.document, game.error));
  CHECK(game.document.node(0).name == "Live unsaved edit");
}
TEST_CASE("scene recovery retains an external conflict after recovery and a second crash") {
  RecoveryGame game;
  SceneRecovery recovery;
  recovery.watch(game.source);
  game.document.set_name(0, "Recovered");
  game.autosave(recovery);
  auto opened = SceneDocument::open(game.source);
  auto& external = std::get<SceneDocument>(opened);
  external.set_name(0, "External");
  REQUIRE(external.save(game.error));
  recovery.watch(game.source);
  REQUIRE(recovery.source_changed());
  REQUIRE(recovery.recover(game.document, game.error));
  game.document.set_name(0, "Recovered again");
  game.autosave(recovery);
  recovery.watch(game.source);
  CHECK(recovery.source_changed());
}
TEST_CASE("scene recovery suppresses a permanent failure until the document changes") {
  RecoveryGame game;
  std::size_t attempts = 0;
  SceneRecovery recovery([&](std::FILE*) {
    ++attempts;
    return false;
  });
  recovery.watch(game.source);
  game.document.set_name(0, "Edit");
  CHECK_FALSE(recovery.poll(game.document, SceneRecovery::kIntervalSeconds, game.error));
  CHECK_FALSE(recovery.problem().empty());
  REQUIRE(recovery.poll(game.document, SceneRecovery::kIntervalSeconds, game.error));
  CHECK(attempts == 1);
  game.document.set_name(0, "Another edit");
  CHECK_FALSE(recovery.poll(game.document, SceneRecovery::kIntervalSeconds, game.error));
  CHECK(attempts == 2);
}
TEST_CASE("editor recovery refuses to replace newer edits made while the prompt was open") {
  RecoveryGame game;
  auto opened = EditorSession::open(game.folder);
  auto session = std::move(std::get<std::unique_ptr<EditorSession>>(opened));
  session->document().set_name(0, "Backup");
  session->autosave(SceneRecovery::kIntervalSeconds);
  session.reset();
  opened = EditorSession::open(game.folder);
  session = std::move(std::get<std::unique_ptr<EditorSession>>(opened));
  REQUIRE(session->recovery_pending());
  session->document().set_name(0, "Newer bridge edit");
  CHECK_FALSE(session->recover_scene(game.error));
  CHECK(session->document().node(0).name == "Newer bridge edit");
  CHECK(session->recovery_pending());
}

TEST_CASE("scene recovery uses the validated offer even if the pathname changes before recovery") {
  RecoveryGame game;
  SceneRecovery recovery;
  recovery.watch(game.source);
  game.document.set_name(0, "Validated backup");
  game.autosave(recovery);
  recovery.watch(game.source);
  CHECK_FALSE(recovery.source_changed());  // cache the exact validated snapshot shown to the user
  REQUIRE(write_text_file(SceneRecovery::copy_path(game.source), "replaced after prompt", game.error));
  REQUIRE(recovery.recover(game.document, game.error));
  CHECK(game.document.node(0).name == "Validated backup");
}

TEST_CASE("a dismissed old recovery copy is not reoffered after a later editor restart") {
  RecoveryGame game;
  SceneRecovery recovery;
  recovery.watch(game.source);
  game.document.set_name(0, "Older backup");
  game.autosave(recovery);
  game.document.set_name(0, "Newer manual save");
  REQUIRE(game.document.save(game.error));
  recovery.ignore();  // the fallback when deleting the older backup fails
  recovery.stop();
  SceneRecovery restarted;
  restarted.watch(game.source);
  CHECK_FALSE(restarted.pending());
  CHECK(fs::exists(SceneRecovery::copy_path(game.source)));
  game.document.set_name(0, "New unsaved edit");
  game.autosave(restarted);
  restarted.watch(game.source);
  CHECK(restarted.pending());  // a fresh backup is not hidden by the old dismissal
}

TEST_CASE("a failed backup cleanup after Save never offers the stale copy over the saved scene") {
  RecoveryGame game;
  auto opened = EditorSession::open(game.folder);
  auto session = std::move(std::get<std::unique_ptr<EditorSession>>(opened));
  session->document().set_name(0, "Older backup");
  session->autosave(SceneRecovery::kIntervalSeconds);
  const auto copy = SceneRecovery::copy_path(game.source);
  REQUIRE(fs::remove(copy));
  REQUIRE(fs::create_directory(copy));
  REQUIRE(write_text_file(copy / "cannot_remove_nonempty_directory", "occupied", game.error));
  session->document().set_name(0, "Newer saved scene");
  REQUIRE(session->save(game.error));
  CHECK_FALSE(session->recovery_pending());
  CHECK_FALSE(session->document().dirty());
  CHECK_FALSE(session->recovery_problem().empty());
  const auto saved = SceneDocument::open(game.source);
  REQUIRE(std::holds_alternative<SceneDocument>(saved));
  CHECK(std::get<SceneDocument>(saved).node(0).name == "Newer saved scene");
}
