#include <doctest/doctest.h>

#include <filesystem>
#include <chrono>

#include "app/scene_files.hpp"
#include "editor/document/glb_file.hpp"
#include "core/config.hpp"
#include "scene/props.hpp"
#include "editor/editor_session.hpp"
#include "core/files.hpp"
#include "editor/scripts/script_files.hpp"
#include "testing/recording_audio.hpp"
#include "test_support.hpp"

using namespace redgem;
using redgem::test::fixture;

namespace {

constexpr int kSettleFrames = 120;
constexpr float kPositionTolerance = 0.05f;
constexpr float kRaisedCrateY = 8.0f;
constexpr float kCrateRestHeight = 0.5f;
constexpr float kMinScriptRise = 1.0f;
constexpr int kExpectedScriptErrors = 2;  // the sandbox's missing and broken scripts
// Pushes a file's modification time forward, as a later write by another program would.
constexpr std::chrono::seconds kLaterWrite{5};

void touch_later(const std::filesystem::path& path) {
  std::filesystem::last_write_time(path, std::filesystem::last_write_time(path) + kLaterWrite);
}

// Each test edits its own copy of the sandbox game.
std::filesystem::path copy_sandbox(const std::string& name) {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / ("redgem_editor_" + name);
  std::filesystem::remove_all(dir);
  std::filesystem::copy(fixture("games/sandbox"), dir, std::filesystem::copy_options::recursive);
  return dir;
}

std::unique_ptr<EditorSession> open(const std::filesystem::path& dir) {
  auto opened = EditorSession::open(dir);
  if (auto* error = std::get_if<std::string>(&opened)) FAIL(*error);
  return std::move(std::get<std::unique_ptr<EditorSession>>(opened));
}

void play_frames(EditorSession& session, int frames) {
  for (int i = 0; i < frames; ++i) session.tick(config::kFixedDeltaSeconds);
}

float y_of(const EditorSession& session, const std::string& name) {
  const ObjectId id = session.scene().find(name);
  REQUIRE(id != kNoObject);
  return session.scene().world_transform(id).position.y;
}

}  // namespace

TEST_CASE("refreshing one node gives the preview what rebuilding it all would, and keeps who it is") {
  constexpr Vec3 kDragged{3.0f, 4.0f, 5.0f};
  const auto dir = copy_sandbox("refresh_node");
  auto session = open(dir);
  const ObjectId crate = session->scene().find("Crate");
  REQUIRE(crate != kNoObject);
  const ObjectSerial serial = session->scene().at(crate).serial;
  Transform local = session->document().node(crate).local;
  local.position = kDragged;
  session->document().set_local(crate, local, false);  // as a gizmo drag does, every frame
  session->refresh_node(crate);
  const GameObject one = session->scene().at(crate);
  CHECK(one.serial == serial);
  session->refresh_preview();
  const GameObject& all = session->scene().at(crate);
  CHECK(approx_equal(one.local.position, all.local.position, kPositionTolerance));
  CHECK(approx_equal(one.local.position, kDragged, kPositionTolerance));
  CHECK(one.name == all.name);
  CHECK(one.parent == all.parent);
  CHECK(one.children == all.children);
  CHECK(one.props == all.props);
  CHECK(one.mesh == all.mesh);
  CHECK(one.prefab == all.prefab);
  CHECK(one.light.has_value() == all.light.has_value());
  std::filesystem::remove_all(dir);
}

TEST_CASE("a game's music, volume and music pitch end with the game, whether Stop ends it or the game quits") {
  test::LogCapture quiet;
  const auto dir = copy_sandbox("music");
  auto session = open(dir);
  ScriptedInputSource input;
  testing::RecordingAudio audio;
  std::string error;
  for (const bool quits_by_itself : {true, false}) {
    CAPTURE(quits_by_itself);
    REQUIRE_MESSAGE(session->play({input, &audio}, error), error);
    REQUIRE(session->runtime()
                ->scripts()
                .eval("Audio.music(:beep); Audio.volume = 0.25; Audio.music_volume = 0.5; Audio.music_pitch = 2.0",
                      "music.rb")
                .ok);
    play_frames(*session, 1);
    REQUIRE(audio.music() == std::optional<std::string>("beep"));
    REQUIRE(audio.master_volume() < kFullVolume);
    REQUIRE(audio.music_pitch() > kAsRecorded);
    if (quits_by_itself) {
      REQUIRE(session->runtime()->scripts().eval("Game.quit", "quit.rb").ok);
      play_frames(*session, 1);
    } else {
      session->stop();
    }
    CHECK(session->play_state() == PlayState::Editing);
    CHECK_FALSE(audio.music().has_value());
    CHECK(audio.master_volume() == kFullVolume);
    CHECK(audio.music_volume() == kFullVolume);
    CHECK(audio.music_pitch() == kAsRecorded);
  }
  std::filesystem::remove_all(dir);
}

TEST_CASE("play runs the edited scene and stop restores it") {
  test::LogCapture quiet;
  const auto dir = copy_sandbox("play");
  auto session = open(dir);
  session->select_by_name("Crate");
  REQUIRE(session->selection() != kNoObject);
  Transform raised = session->document().node(session->selection()).local;
  raised.position.y = kRaisedCrateY;
  session->document().set_local(session->selection(), raised);
  session->refresh_preview();

  ScriptedInputSource input;
  std::string error;
  REQUIRE_MESSAGE(session->play({input}, error), error);
  CHECK(session->play_state() == PlayState::Playing);
  CHECK(y_of(*session, "Crate") == doctest::Approx(kRaisedCrateY));  // starts from the edit
  play_frames(*session, kSettleFrames);
  CHECK(y_of(*session, "Crate") == doctest::Approx(kCrateRestHeight).epsilon(kPositionTolerance));

  session->duplicate_selection();  // editing is locked while playing
  CHECK(session->document().desc().find_node("Crate.001") == kNoIndex);

  session->stop();
  CHECK(session->play_state() == PlayState::Editing);
  CHECK(session->runtime() == nullptr);
  CHECK(y_of(*session, "Crate") == doctest::Approx(kRaisedCrateY));
  std::filesystem::remove_all(dir);
}

TEST_CASE("pause freezes the game") {
  test::LogCapture quiet;
  const auto dir = copy_sandbox("pause");
  auto session = open(dir);
  ScriptedInputSource input;
  std::string error;
  REQUIRE(session->play({input}, error));
  play_frames(*session, 1);
  session->pause(true);
  const float frozen = y_of(*session, "Crate");
  play_frames(*session, kSettleFrames);
  CHECK(y_of(*session, "Crate") == doctest::Approx(frozen));
  session->pause(false);
  play_frames(*session, kSettleFrames);
  CHECK(y_of(*session, "Crate") < frozen);
  std::filesystem::remove_all(dir);
}

TEST_CASE("selection follows duplicate, delete and undo") {
  const auto dir = copy_sandbox("selection");
  auto session = open(dir);
  session->select_by_name("Crate");
  session->duplicate_selection();
  CHECK(session->document().node(session->selection()).name == "Crate.001");
  CHECK(session->scene().find("Crate.001") != kNoObject);  // preview refreshed
  session->delete_selection();
  CHECK(session->selection() == kNoObject);
  CHECK(session->scene().find("Crate.001") == kNoObject);
  session->undo();
  CHECK(session->scene().find("Crate.001") != kNoObject);
  session->redo();
  CHECK(session->scene().find("Crate.001") == kNoObject);
  std::filesystem::remove_all(dir);
}

TEST_CASE("undo keeps the same object selected even when indices shift") {
  const auto dir = copy_sandbox("undo_selection");
  auto session = open(dir);
  session->select_by_name("Ground");  // an early node: deleting it shifts every later index
  session->delete_selection();
  session->select_by_name("Camera");
  session->undo();
  REQUIRE(session->selection() != kNoObject);
  CHECK(session->document().node(session->selection()).name == "Camera");
  session->redo();
  REQUIRE(session->selection() != kNoObject);
  CHECK(session->document().node(session->selection()).name == "Camera");
  std::filesystem::remove_all(dir);
}

TEST_CASE("outside changes wait for a decision when there are unsaved edits") {
  test::LogCapture quiet;
  const auto dir = copy_sandbox("pending");
  auto session = open(dir);
  session->select_by_name("Crate");
  session->document().set_name(session->selection(), "Mine");
  session->refresh_preview();
  touch_later(dir / "scene.glb");
  CHECK_FALSE(session->reload_if_changed_on_disk());
  CHECK(session->external_change_pending());
  CHECK(session->scene().find("Mine") != kNoObject);  // edits are kept until the user decides
  REQUIRE(session->reload_from_disk());
  CHECK_FALSE(session->external_change_pending());
  CHECK(session->scene().find("Crate") != kNoObject);
  CHECK_FALSE(session->document().dirty());
  std::filesystem::remove_all(dir);
}

TEST_CASE("saved edits are used by the next run and reloaded from disk") {
  test::LogCapture quiet;
  const auto dir = copy_sandbox("reload");
  auto session = open(dir);
  session->select_by_name("Crate");
  session->document().set_name(session->selection(), "Box");
  std::string error;
  REQUIRE_MESSAGE(session->save(error), error);
  CHECK_FALSE(session->reload_if_changed_on_disk());  // our own save is not a foreign change

  // Another editor (e.g. Blender re-exporting) rewrites the file.
  auto other = open(dir);
  other->select_by_name("Box");
  other->document().set_name(other->selection(), "Barrel");
  REQUIRE(other->save(error));
  touch_later(dir / "scene.glb");

  session->select_by_name("Box");
  CHECK(session->reload_if_changed_on_disk());
  CHECK(session->scene().find("Barrel") != kNoObject);
  std::filesystem::remove_all(dir);
}

TEST_CASE("a script written in the editor runs when the game plays") {
  test::LogCapture quiet;
  const auto dir = copy_sandbox("custom_script");
  auto session = open(dir);
  std::string error;
  const auto file = create_script(dir / "scripts", "floater", error);
  REQUIRE_MESSAGE(file.has_value(), error);
  // What a user would type in the Script panel.
  REQUIRE(write_text_file(dir / "scripts" / *file,
                          "class Floater < Behaviour\n"
                          "  RISE_SPEED = 2.0\n"
                          "  def update(dt)\n"
                          "    transform.translate(Vector3.up * RISE_SPEED * dt)\n"
                          "  end\n"
                          "end\n",
                          error));
  session->select_by_name("Ground");  // a static object: only the script moves it
  session->document().set_prop(session->selection(), "script", *file);
  session->refresh_preview();
  const float start_y = y_of(*session, "Ground");

  ScriptedInputSource input;
  REQUIRE(session->play({input}, error));
  play_frames(*session, kSettleFrames);
  CHECK(y_of(*session, "Ground") > start_y + kMinScriptRise);
  CHECK(session->runtime()->scripts().error_count() == kExpectedScriptErrors);  // only the sandbox's known ones
  session->stop();
  CHECK(y_of(*session, "Ground") == doctest::Approx(start_y));
  std::filesystem::remove_all(dir);
}

TEST_CASE("objects created from the menus are placed, selected and undoable") {
  constexpr Vec3 kSpot{1.0f, 2.0f, 3.0f};
  const std::filesystem::path dir = copy_sandbox("create");
  auto session = open(dir);
  const int count = session->document().node_count();
  const ObjectId cube = session->create_primitive(PrimitiveShape::Cube, kSpot);
  REQUIRE(cube != kNoObject);
  CHECK(session->selection() == cube);
  CHECK(session->document().node(cube).name == "Cube");
  CHECK(approx_equal(session->scene().world_transform(cube).position, kSpot, kPositionTolerance));
  const ObjectId sun = session->create_light(LightType::Directional, kSpot);
  REQUIRE(session->document().node(sun).light.has_value());
  CHECK(session->scene().world_transform(sun).forward().y < 0.0f);  // shines down
  session->create_camera(kSpot);
  session->create_empty(kSpot);
  CHECK(session->document().node_count() == count + 4);
  session->undo();  // one step per created object, position included
  CHECK(session->document().node_count() == count + 3);

  // Reparenting from the hierarchy keeps the selection on the same object.
  session->select(cube);
  REQUIRE(session->scene().find("Crate") != kNoObject);
  session->reparent(cube, session->scene().find("Crate"));
  const ObjectId moved = session->scene().find("Cube");
  CHECK(session->selection() == moved);
  CHECK(session->document().node(moved).parent == session->scene().find("Crate"));
  CHECK(approx_equal(session->scene().world_transform(moved).position, kSpot, kPositionTolerance));

  session->attach_script(moved, "lift.rb");
  session->attach_script(moved, "kid.rb");
  session->attach_script(moved, "lift.rb");  // already there
  CHECK(get_string(session->document().node(moved).props, props::kScript) == "lift.rb, kid.rb");
  std::filesystem::remove_all(dir);
}

TEST_CASE("a model from models/ is imported into the scene and plays") {
  constexpr Vec3 kSpot{0.0f, 5.0f, 0.0f};
  const std::filesystem::path dir = copy_sandbox("import");
  std::filesystem::create_directories(dir / config::kModelsDirName);
  std::filesystem::copy_file(fixture("hierarchy.glb"), dir / config::kModelsDirName / "hierarchy.glb");
  auto session = open(dir);
  std::string error;
  const ObjectId model = session->import_model(dir / config::kModelsDirName / "hierarchy.glb", kSpot, error);
  REQUIRE_MESSAGE(model != kNoObject, error);
  CHECK(session->selection() == model);
  REQUIRE(session->save(error));
  ScriptedInputSource input;
  REQUIRE(session->play({input}, error));
  CHECK(session->scene().find("Child") != kNoObject);
  session->stop();
  std::filesystem::remove_all(dir);
}

TEST_CASE("a model dropped on an object becomes its child at the dropped world position") {
  constexpr Vec3 kSpot{2.0f, 0.0f, -3.0f};
  const std::filesystem::path dir = copy_sandbox("import_child");
  auto session = open(dir);
  const ObjectId crate = session->scene().find("Crate");
  REQUIRE(crate != kNoObject);
  const int steps_before = session->document().can_undo() ? 1 : 0;
  std::string error;
  const ObjectId model = session->import_model(fixture("hierarchy.glb"), kSpot, error, crate);
  REQUIRE_MESSAGE(model != kNoObject, error);
  CHECK(session->document().node(model).parent == crate);
  CHECK(approx_equal(session->scene().world_transform(model).position, kSpot, kPositionTolerance));
  session->undo();  // a single step removes it
  CHECK(session->scene().find("hierarchy") == kNoObject);
  CHECK((session->document().can_undo() ? 1 : 0) == steps_before);
  std::filesystem::remove_all(dir);
}

TEST_CASE("the environment object carries the scene's sky and effects, and there is only one") {
  const std::filesystem::path dir = copy_sandbox("environment");
  auto session = open(dir);
  const int count = session->document().node_count();
  const ObjectId environment = session->create_environment();
  REQUIRE(environment != kNoObject);
  CHECK(session->selection() == environment);
  const PropertyMap& props = session->document().node(environment).props;
  CHECK(get_bool(props, "environment") == true);
  CHECK(get_string(props, "sky_horizon").has_value());
  CHECK(get_number(props, "bloom").has_value());
  CHECK(session->create_environment() == environment);  // selects the one that exists
  CHECK(session->document().node_count() == count + 1);
  session->undo();  // one step
  CHECK(session->document().node_count() == count);
  std::filesystem::remove_all(dir);
}

TEST_CASE("another scene of the game opens with its own undo history, and plays as itself") {
  constexpr const char* kSecondScene = "level2";
  const auto dir = copy_sandbox("open_scene");
  auto session = open(dir);
  CHECK(session->scene_name() == config::kMainSceneName);
  session->select_by_name("Crate");
  session->create_empty(Vec3::zero());  // an undo step of scene.glb
  std::string error;
  REQUIRE_MESSAGE(session->open_scene(kSecondScene, error), error);
  CHECK(session->scene_name() == kSecondScene);
  CHECK(session->document().path() == dir / config::kScenesDirName / "level2.glb");
  CHECK_FALSE(session->document().can_undo());  // nothing of the last scene's history
  CHECK_FALSE(session->document().dirty());
  CHECK(session->selection() == kNoObject);
  CHECK(session->scene().find("Level2Marker") != kNoObject);

  // Play starts from the open scene, by its name, and Stop comes back to it.
  ScriptedInputSource input;
  REQUIRE_MESSAGE(session->play({input}, error), error);
  CHECK(session->runtime()->scene_name() == kSecondScene);
  CHECK(session->scene().find("Level2Marker") != kNoObject);
  CHECK_FALSE(session->open_scene(config::kMainSceneName, error));  // not while the game runs
  session->stop();
  CHECK(session->scene_name() == kSecondScene);
  CHECK(session->scene().find("Level2Marker") != kNoObject);
  std::filesystem::remove_all(dir);
}

TEST_CASE("save writes the open scene back to its own file, and save as makes another") {
  const auto dir = copy_sandbox("save_scene");
  auto session = open(dir);
  std::string error;
  REQUIRE(session->open_scene("level2", error));
  session->create_empty(Vec3::zero());
  REQUIRE(session->save(error));
  CHECK(read_scene(dir, "level2")->find_node("GameObject") != kNoIndex);
  CHECK(read_scene(dir, config::kMainSceneName)->find_node("GameObject") == kNoIndex);

  session->create_camera(Vec3::zero());
  REQUIRE_MESSAGE(session->save_as("level3", error), error);
  CHECK(session->scene_name() == "level3");
  CHECK_FALSE(session->document().dirty());
  CHECK(read_scene(dir, "level3")->find_node("Camera.001") != kNoIndex);
  CHECK(read_scene(dir, "level2")->find_node("Camera.001") == kNoIndex);
  CHECK_FALSE(session->save_as("level2", error));  // a scene of the game already
  std::filesystem::remove_all(dir);
}

TEST_CASE("the open scene's file is the one followed for outside changes") {
  const auto dir = copy_sandbox("follow_scene");
  auto session = open(dir);
  std::string error;
  REQUIRE(session->open_scene("level2", error));
  touch_later(dir / config::kSceneFileName);
  CHECK_FALSE(session->reload_if_changed_on_disk());  // not the scene being edited
  touch_later(dir / config::kScenesDirName / "level2.glb");
  CHECK(session->reload_if_changed_on_disk());
  CHECK(session->scene_name() == "level2");
  std::filesystem::remove_all(dir);
}

TEST_CASE("save as refuses to move a scene away from the files next to it that it uses") {
  constexpr const char* kLinkedImage = "wood.png";
  const auto dir = copy_sandbox("save_as_linked");
  const std::filesystem::path scene = dir / config::kSceneFileName;
  std::string error;
  auto glb = read_glb(scene, error);
  REQUIRE_MESSAGE(glb, error);
  glb->json["images"] = nlohmann::json::array({{{"uri", kLinkedImage}}});  // a texture kept next to it
  REQUIRE(write_glb(scene, *glb, error));
  REQUIRE(write_text_file(dir / kLinkedImage, "not really a picture", error));
  auto session = open(dir);
  CHECK(session->document().linked_files() == std::vector<std::string>{kLinkedImage});

  session->create_empty(Vec3::zero());
  CHECK_FALSE(session->save_as("level3", error));  // scenes/ would not find scenes/wood.png
  CHECK(error.find(kLinkedImage) != std::string::npos);
  CHECK_FALSE(std::filesystem::exists(dir / config::kScenesDirName / "level3.glb"));
  CHECK(session->scene_name() == config::kMainSceneName);
  CHECK(session->document().dirty());
  CHECK(session->save(error));  // in its own folder it still finds it
  std::filesystem::remove_all(dir);
}

TEST_CASE("several selected objects are duplicated, moved and deleted together, one undo step each") {
  constexpr Vec3 kLift{0.0f, 3.0f, 0.0f};
  const auto dir = copy_sandbox("multi_select");
  auto session = open(dir);
  const ObjectId crate = session->scene().find("Crate");
  const ObjectId coin = session->scene().find("Coin");
  const ObjectId kid = session->scene().find("Kid");
  const ObjectId family = session->scene().find("Family");
  session->click_select(crate, ClickMode::Replace);
  session->click_select(coin, ClickMode::Toggle);
  session->click_select(kid, ClickMode::Toggle);
  session->click_select(family, ClickMode::Toggle);
  REQUIRE(session->selected().size() == 4);
  CHECK(session->selected_roots().size() == 3);  // Kid goes along with Family

  session->duplicate_selection();
  CHECK(session->selected().size() == 3);  // the copies
  CHECK(session->scene().find("Crate.001") != kNoObject);
  CHECK(session->scene().find("Kid.001") != kNoObject);
  session->undo();
  CHECK(session->scene().find("Crate.001") == kNoObject);
  CHECK(session->selected().empty());  // the copies were selected, and are gone

  // Moved together as a drag does: one checkpoint, then every object placed.
  session->select_all({crate, coin});
  const float crate_y = y_of(*session, "Crate");
  const float coin_y = y_of(*session, "Coin");
  session->document().checkpoint();
  for (const ObjectId id : session->selected_roots()) {
    Transform world = session->scene().world_transform(id);
    world.position = world.position + kLift;
    session->place_in_world(id, world);
  }
  CHECK(y_of(*session, "Crate") == doctest::Approx(crate_y + kLift.y));
  CHECK(y_of(*session, "Coin") == doctest::Approx(coin_y + kLift.y));
  session->undo();
  CHECK(y_of(*session, "Crate") == doctest::Approx(crate_y));
  CHECK(y_of(*session, "Coin") == doctest::Approx(coin_y));
  CHECK(session->selected().size() == 2);  // undo keeps both selected

  session->delete_selection();
  CHECK(session->scene().find("Crate") == kNoObject);
  CHECK(session->scene().find("Coin") == kNoObject);
  session->undo();
  CHECK(session->scene().find("Crate") != kNoObject);
  CHECK(session->scene().find("Coin") != kNoObject);
  std::filesystem::remove_all(dir);
}

TEST_CASE("objects cut from one scene are pasted into another, and saved there") {
  const auto dir = copy_sandbox("cut_paste");
  auto session = open(dir);
  session->select_all({session->scene().find("Crate"), session->scene().find("Family")});
  std::string error;
  const auto clip = session->cut_selection(error);
  REQUIRE_MESSAGE(clip, error);
  CHECK(session->scene().find("Crate") == kNoObject);
  CHECK(session->scene().find("Kid") == kNoObject);
  session->undo();  // the cut is one undo step
  CHECK(session->scene().find("Kid") != kNoObject);

  REQUIRE_MESSAGE(session->open_scene("level2", error), error);
  session->select_by_name("Level2Marker");
  REQUIRE_MESSAGE(session->paste(*clip, session->paste_parent(), error), error);
  CHECK(session->selected().size() == 2);
  const ObjectId kid = session->scene().find("Kid");
  REQUIRE(kid != kNoObject);
  CHECK(session->document().node(kid).parent == session->scene().find("Family"));
  CHECK(session->scene().find("Crate") != kNoObject);
  REQUIRE_MESSAGE(session->save(error), error);

  auto reopened = open(dir);
  REQUIRE_MESSAGE(reopened->open_scene("level2", error), error);
  const ObjectId crate = reopened->scene().find("Crate");
  REQUIRE(crate != kNoObject);
  CHECK(get_string(reopened->document().node(crate).props, props::kTag) == std::optional<std::string>("crate"));
  CHECK(reopened->document().node(crate).mesh != kNoIndex);
  std::filesystem::remove_all(dir);
}

TEST_CASE("pasting under an object puts the copies there, and nothing is pasted while playing") {
  const auto dir = copy_sandbox("paste_into");
  auto session = open(dir);
  session->select_by_name("Coin");
  std::string error;
  const auto clip = session->copy_selection(error);
  REQUIRE_MESSAGE(clip, error);
  const ObjectId family = session->scene().find("Family");
  REQUIRE_MESSAGE(session->paste(*clip, family, error), error);
  const ObjectId copy = session->scene().find("Coin.001");
  REQUIRE(copy != kNoObject);
  CHECK(session->document().node(copy).parent == family);
  CHECK(session->selection() == copy);

  ScriptedInputSource input;
  REQUIRE_MESSAGE(session->play({input}, error), error);
  error.clear();
  CHECK_FALSE(session->paste(*clip, kNoObject, error));
  session->stop();
  std::filesystem::remove_all(dir);
}

TEST_CASE("the Inspector of several objects moves a selected child once, with its parent") {
  constexpr float kNewX = 7.0f;
  constexpr float kMinScale = 0.001f;
  const auto dir = copy_sandbox("group_inspector");
  auto session = open(dir);
  const ObjectId family = session->scene().find("Family");
  const ObjectId kid = session->scene().find("Kid");
  const Vec3 kid_local = session->document().node(kid).local.position;
  session->select_all({family, kid, session->scene().find("Crate")});
  session->document().checkpoint();
  session->set_roots_axis(inspector_logic::TransformPart::Position, 0, kNewX, kMinScale);
  CHECK(session->document().node(family).local.position.x == doctest::Approx(kNewX));
  CHECK(session->document().node(session->scene().find("Crate")).local.position.x == doctest::Approx(kNewX));
  CHECK(
      approx_equal(session->document().node(kid).local.position, kid_local, kPositionTolerance));  // moved with Family
  std::filesystem::remove_all(dir);
}

TEST_CASE("the whole selection is dragged under another object in one undo step") {
  const auto dir = copy_sandbox("group_reparent");
  auto session = open(dir);
  session->select_all({session->scene().find("Crate"), session->scene().find("Coin")});
  session->reparent_selection(session->scene().find("Family"));
  const ObjectId family = session->scene().find("Family");
  CHECK(session->document().node(session->scene().find("Crate")).parent == family);
  CHECK(session->document().node(session->scene().find("Coin")).parent == family);
  CHECK(session->selected().size() == 2);
  session->undo();
  CHECK(session->document().node(session->scene().find("Crate")).parent == kNoIndex);
  CHECK(session->document().node(session->scene().find("Coin")).parent == kNoIndex);
  std::filesystem::remove_all(dir);
}
