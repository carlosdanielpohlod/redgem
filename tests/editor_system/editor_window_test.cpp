// System test: opens the real editor window on a copy of the platformer and drives it.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <filesystem>

#include <imgui.h>
#include <imgui_internal.h>

#include "app/bundled_game.hpp"
#include "app/game_settings.hpp"
#include "core/files.hpp"
#include "core/platform.hpp"
#include "project/editor_setup.hpp"
#include "project/game_build.hpp"
#include "project/game_templates.hpp"
#include "project/recent_games.hpp"
#include "core/config.hpp"
#include "scene/props.hpp"
#include "editor/editor_app.hpp"
#include "project/project_hub.hpp"
#include "editor/editor_config.hpp"
#include "editor/script_files.hpp"
#include "editor/ui/panels.hpp"
#include "scene/gltf_loader.hpp"
#include "window_picture.hpp"

using namespace redgem;
using redgem::test::save_window_picture;

namespace {

constexpr int kWarmupFrames = 10;
constexpr int kFrameFrames = 5;
constexpr int kPictureFrames = 3;  // more than the buffers a window swaps between
constexpr int kPlayFrames = 90;
constexpr float kLift = 3.0f;
constexpr float kTolerance = 1e-3f;
constexpr float kViewportCenter = 0.5f;
constexpr float kMinSpin = 1.0f;  // degrees
constexpr const char* kHierarchyWindowName = editor_config::kHierarchyWindow;

std::filesystem::path screenshot(const std::string& name) {
  return std::filesystem::path(REDGEM_SCREENSHOT_DIR) / "editor" / (name + ".png");
}

std::filesystem::path copy_game() {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "redgem_editor_window";
  std::filesystem::remove_all(dir);
  std::filesystem::copy(REDGEM_PLATFORMER_DIR, dir, std::filesystem::copy_options::recursive);
  return dir;
}

// Every frame counts as one fixed step, however long the machine took to draw it.
EditorOptions even_pace() {
  EditorOptions options = editor_options(GpuChoice::Auto);
  options.clock = [] { return config::kFixedDeltaSeconds; };
  return options;
}

void run(EditorApp& app, int frames) {
  for (int i = 0; i < frames; ++i) app.frame();
}

// Writes a picture of the window (an editor's or the hub's) for whoever looks at the run afterwards.
// The window is read after its frame was shown, from the buffer it draws the next one into: that
// holds one of the last few frames, so as many equal ones are drawn first.
template <typename Window>
void take_picture(Window& window, const std::filesystem::path& file) {
  for (int i = 0; i < kPictureFrames; ++i) window.frame();
  CHECK(save_window_picture(file));
}

float y_of(EditorSession& session, const std::string& name) {
  const ObjectId id = session.scene().find(name);
  REQUIRE(id != kNoObject);
  return session.scene().world_transform(id).position.y;
}

}  // namespace

TEST_CASE("the editor opens, edits, plays, stops and saves a game") {
  const std::filesystem::path game = copy_game();
  auto created = EditorApp::create(game, even_pace());  // the game advances the same on any machine
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  EditorApp& app = *std::get<std::unique_ptr<EditorApp>>(created);
  EditorSession& session = app.session();
  run(app, kWarmupFrames);
  take_picture(app, screenshot("editor_open"));

  // Edit like the inspector does: lift the crate one undo step.
  session.select_by_name("Crate1");
  REQUIRE(session.selection() != kNoObject);
  const float original_y = y_of(session, "Crate1");
  Transform lifted = session.document().node(session.selection()).local;
  lifted.position.y += kLift;
  session.document().set_local(session.selection(), lifted);
  session.refresh_preview();
  run(app, kFrameFrames);
  CHECK(y_of(session, "Crate1") == doctest::Approx(original_y + kLift));
  take_picture(app, screenshot("editor_edited"));

  // Play: physics drops the lifted crate. Stop: back to the edited scene.
  app.play();
  REQUIRE(session.play_state() == PlayState::Playing);
  run(app, kPlayFrames);
  take_picture(app, screenshot("editor_playing"));
  CHECK(y_of(session, "Crate1") < original_y + kLift - kTolerance);
  app.stop();
  CHECK(session.play_state() == PlayState::Editing);
  CHECK(y_of(session, "Crate1") == doctest::Approx(original_y + kLift));

  // Save, then the file on disk has the edit.
  app.save();
  CHECK_FALSE(session.document().dirty());
  const LoadResult saved = load_gltf_scene((game / "scene.glb").string());
  REQUIRE(saved.ok);
  const int crate = saved.scene.find_node("Crate1");
  REQUIRE(crate != kNoIndex);
  CHECK(saved.scene.nodes[crate].local.position.y == doctest::Approx(original_y + kLift));

  // Undo goes back to the original position.
  session.undo();
  CHECK(y_of(session, "Crate1") == doctest::Approx(original_y));
  run(app, kFrameFrames);

  CHECK(app.console().error_count() == 0);
  std::filesystem::remove_all(game);
}

TEST_CASE("clicking an object in the viewport selects it") {
  const std::filesystem::path game = copy_game();
  auto created = EditorApp::create(game, editor_options(GpuChoice::Auto));
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  EditorApp& app = *std::get<std::unique_ptr<EditorApp>>(created);
  EditorSession& session = app.session();
  run(app, kWarmupFrames);

  session.select_by_name("Coin1");
  app.frame_selection();
  session.select(kNoObject);
  run(app, kFrameFrames);
  // The framed object sits at the center of the viewport.
  const ObjectId picked = app.pick_at(app.viewport_width() * kViewportCenter, app.viewport_height() * kViewportCenter);
  REQUIRE(picked != kNoObject);
  CHECK(session.document().node(picked).name == "Coin1");
  CHECK(session.selection() == picked);
  take_picture(app, screenshot("editor_picked"));
  std::filesystem::remove_all(game);
}

TEST_CASE("a new Ruby script is written in the Script panel and runs on Play") {
  const std::filesystem::path game = copy_game();
  auto created = EditorApp::create(game, editor_options(GpuChoice::Auto));
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  EditorApp& app = *std::get<std::unique_ptr<EditorApp>>(created);
  EditorSession& session = app.session();
  run(app, kWarmupFrames);

  std::string error;
  const auto file = create_script(app.scripts().scripts_dir(), "spin_tree", error);
  REQUIRE_MESSAGE(file.has_value(), error);
  ScriptEditor* editor = app.scripts().open(*file, error);
  REQUIRE(editor != nullptr);
  editor->text() =
      "class SpinTree < Behaviour\n"
      "  DEGREES_PER_SECOND = 90.0\n"
      "  def update(dt)\n"
      "    transform.rotate(Vector3.new(0, DEGREES_PER_SECOND * dt, 0))\n"
      "  end\n"
      "end\n";
  session.select_by_name("Tree1");
  session.document().set_prop(session.selection(), "script", *file);
  session.refresh_preview();
  run(app, kFrameFrames);
  take_picture(app, screenshot("editor_script_panel"));

  app.play();  // saves the open script first
  CHECK_FALSE(editor->modified());
  CHECK(editor->syntax_error().empty());
  run(app, kPlayFrames);
  const ObjectId tree = session.scene().find("Tree1");
  const Vec3 euler = quat_to_euler_degrees(session.scene().world_transform(tree).rotation);
  CHECK(std::fabs(euler.y) > kMinSpin);
  app.stop();
  CHECK(app.console().error_count() == 0);
  std::filesystem::remove_all(game);
}

TEST_CASE("a game is built up in the editor: new objects, an imported model, materials, then a standalone build") {
  constexpr Vec3 kGlow{3.0f, 0.4f, 0.1f};
  constexpr float kMetal = 1.0f;
  const std::filesystem::path game = copy_game();
  const std::filesystem::path models = game / config::kModelsDirName;
  std::filesystem::create_directories(models);
  std::filesystem::copy_file(std::filesystem::path(REDGEM_FIXTURES_DIR) / "hierarchy.glb", models / "tower.glb");
  std::string error;
  // The model comes with a script property (spinner.rb): the game provides that script.
  REQUIRE(create_script(game / config::kScriptsDirName, "spinner", error).has_value());
  auto created = EditorApp::create(game, editor_options(GpuChoice::Auto));
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  EditorApp& app = *std::get<std::unique_ptr<EditorApp>>(created);
  EditorSession& session = app.session();
  run(app, kWarmupFrames);
  REQUIRE(app.files().models == std::vector<std::string>{"tower.glb"});

  // GameObject > 3D Object > Sphere, then a glowing metal material in the Inspector.
  const ObjectId sphere = session.create_primitive(PrimitiveShape::Sphere, app.spawn_point());
  REQUIRE(sphere != kNoObject);
  SceneDocument& doc = session.document();
  MaterialDesc material = doc.material(doc.material_of(sphere));
  material.emissive = kGlow;
  material.metallic = kMetal;
  doc.set_material(doc.material_of(sphere), material);
  session.attach_script(sphere, "coin.rb");
  session.create_light(LightType::Point, app.spawn_point());
  const ObjectId tower = session.import_model(models / "tower.glb", app.spawn_point(), error);
  REQUIRE_MESSAGE(tower != kNoObject, error);
  session.select(sphere);
  run(app, kFrameFrames);
  take_picture(app, screenshot("editor_created"));

  // Plays with the new objects, then builds a copy that runs on its own.
  app.play();
  run(app, kFrameFrames);
  CHECK(session.scene().find("Sphere") != kNoObject);
  app.stop();
  const auto build = app.build();
  REQUIRE(build.has_value());
  CHECK_FALSE(session.document().dirty());  // building saves first
  // What it built is the player under the game's name: not this program, which is the editor.
  const std::filesystem::path built_program = *build / executable_name(game);
  REQUIRE(std::filesystem::exists(built_program));
  CHECK(std::filesystem::file_size(built_program) == std::filesystem::file_size(REDGEM_PLAYER_FILE));
  CHECK(std::filesystem::file_size(built_program) != std::filesystem::file_size(platform::executable_path()));
  const LoadResult built = load_gltf_scene((*build / "game" / "scene.glb").string());
  REQUIRE(built.ok);
  CHECK(built.scene.find_node("Sphere") != kNoIndex);
  CHECK(built.scene.find_node("tower") != kNoIndex);
  CHECK(app.console().error_count() == 0);

  // File > Open Game goes back to the hub once nothing is unsaved.
  app.request_exit(EditorExit::OpenHub);
  CHECK(app.finished());
  CHECK(app.exit_action() == EditorExit::OpenHub);
  std::filesystem::remove_all(*build);
  std::filesystem::remove_all(game);
}

TEST_CASE("leaving with unsaved edits asks first") {
  const std::filesystem::path game = copy_game();
  auto created = EditorApp::create(game, editor_options(GpuChoice::Auto));
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  EditorApp& app = *std::get<std::unique_ptr<EditorApp>>(created);
  run(app, kWarmupFrames);
  app.session().create_empty(app.spawn_point());
  app.request_exit(EditorExit::OpenHub);
  CHECK_FALSE(app.finished());
  run(app, kFrameFrames);
  take_picture(app, screenshot("editor_unsaved_dialog"));
  CHECK_FALSE(app.finished());  // waits for Save / Discard / Cancel
  std::filesystem::remove_all(game);
}

TEST_CASE("another game opens with panels that remember nothing of the last one") {
  const std::filesystem::path game = copy_game();
  {
    auto created = EditorApp::create(game, editor_options(GpuChoice::Auto));
    if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
    EditorApp& app = *std::get<std::unique_ptr<EditorApp>>(created);
    run(app, kWarmupFrames);
    // What a user leaves behind: errors hidden in the Console, a search in the Hierarchy, the
    // Project panel on another folder, a name half typed.
    ui::PanelState& state = app.panel_state();
    state.console.filters.errors = false;
    state.console.filters.info = false;
    state.hierarchy.filter = "Coin";
    state.project.folder = ui::ProjectFolder::Sounds;
    state.inspector.new_property_key = "spe";
    run(app, kFrameFrames);
    CHECK_FALSE(app.panel_state().console.filters.shows(log::Level::Error));
  }
  auto created = EditorApp::create(game, editor_options(GpuChoice::Auto));
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  EditorApp& app = *std::get<std::unique_ptr<EditorApp>>(created);
  run(app, kWarmupFrames);
  const ui::PanelState& state = app.panel_state();
  for (const log::Level level : {log::Level::Info, log::Level::Warning, log::Level::Error}) {
    CHECK(state.console.filters.shows(level));
  }
  CHECK(state.hierarchy.filter.empty());
  CHECK(state.project.folder == ui::ProjectFolder::Scripts);
  CHECK(state.inspector.new_property_key.empty());
  std::filesystem::remove_all(game);
}

TEST_CASE("the project hub creates a game from a template and lists it next time") {
  const std::filesystem::path root = std::filesystem::temp_directory_path() / "redgem_hub";
  std::filesystem::remove_all(root);
  const ProjectHub::Options options{root / "games", root / "recent.json"};
  {
    ProjectHub hub(options);
    for (int i = 0; i < kWarmupFrames; ++i) hub.frame();
    take_picture(hub, screenshot("hub_empty"));
    CHECK(hub.games().empty());
    CHECK_FALSE(hub.create_game("bad/name", "platformer"));
    CHECK_FALSE(hub.error().empty());
    REQUIRE_MESSAGE(hub.create_game("Coin Rush", "platformer"), hub.error());
    REQUIRE(hub.chosen().has_value());
    CHECK(*hub.chosen() == root / "games" / "Coin Rush");
    CHECK(std::filesystem::exists(root / "games" / "Coin Rush" / "scene.glb"));
  }
  std::string error;
  REQUIRE(create_game(root / "games" / "Second", "empty", error));
  ProjectHub hub(options);
  REQUIRE(hub.games().size() == 2);
  CHECK(hub.games().front().filename() == "Coin Rush");  // the recent one first
  for (int i = 0; i < kWarmupFrames; ++i) hub.frame();
  take_picture(hub, screenshot("hub_games"));
  CHECK_FALSE(hub.open_game(root / "nothing_here"));
  CHECK_FALSE(hub.chosen().has_value());
  CHECK(hub.open_game(root / "games" / "Second"));
  std::filesystem::remove_all(root);
}

TEST_CASE("scripts open in tabs that split side by side, and the side panels hide for room") {
  const std::filesystem::path game = copy_game();
  auto created = EditorApp::create(game, editor_options(GpuChoice::Auto));
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  EditorApp& app = *std::get<std::unique_ptr<EditorApp>>(created);
  run(app, kWarmupFrames);
  std::string error;
  REQUIRE(app.scripts().open("player.rb", error) != nullptr);
  run(app, kFrameFrames);
  REQUIRE(app.scripts().open("coin.rb", error) != nullptr);
  run(app, kFrameFrames);
  REQUIRE(app.scripts().tabs().size() == 2);
  app.split_script_right("coin.rb");
  run(app, kFrameFrames);
  // The split tab now lives in its own dock node, apart from player.rb.
  const ImGuiWindow* player = ImGui::FindWindowByName(ui::script_window_name("player.rb").c_str());
  const ImGuiWindow* coin = ImGui::FindWindowByName(ui::script_window_name("coin.rb").c_str());
  REQUIRE(player != nullptr);
  REQUIRE(coin != nullptr);
  CHECK(player->DockId != coin->DockId);
  CHECK(coin->Pos.x > player->Pos.x);
  take_picture(app, screenshot("editor_split_scripts"));

  // Focus on code: hide the hierarchy, the inspector and the bottom panels.
  const float code_width = coin->Size.x + player->Size.x;
  app.panels() = {false, false, false};
  run(app, kFrameFrames);
  CHECK(ImGui::FindWindowByName(kHierarchyWindowName)->Active == false);
  CHECK(coin->Size.x + player->Size.x > code_width);  // the code got the room
  take_picture(app, screenshot("editor_focus_code"));
  app.panels() = {};
  run(app, kFrameFrames);
  CHECK(ImGui::FindWindowByName(kHierarchyWindowName)->Active);
  CHECK(app.console().error_count() == 0);
  std::filesystem::remove_all(game);
}

TEST_CASE("a terrain is created, sculpted with the brush in the Scene view, and things land on its hills") {
  constexpr float kBrushSeconds = 1.0f;
  constexpr float kMinHill = 2.0f;  // meters raised by a second of the default brush
  constexpr float kDropHeight = 12.0f;
  constexpr float kBallRadius = 0.5f;  // half of the 1 m crate
  constexpr float kRestTolerance = 0.4f;
  constexpr int kFallFrames = 240;
  constexpr int kFlattenPasses = 4;
  constexpr float kFullFlatten = 1.0f;
  constexpr float kShoulder = 2.0f;  // meters from the middle of the plateau
  const std::filesystem::path game = copy_game();
  auto created = EditorApp::create(game, even_pace());  // the game advances the same on any machine
  if (auto* error = std::get_if<std::string>(&created)) FAIL(*error);
  EditorApp& app = *std::get<std::unique_ptr<EditorApp>>(created);
  EditorSession& session = app.session();
  run(app, kWarmupFrames);

  // GameObject > Terrain, away from the level; the camera looks down at it.
  const Vec3 place{200.0f, 0.0f, 0.0f};
  const ObjectId terrain = session.create_terrain(place);
  REQUIRE(terrain != kNoObject);
  CHECK(session.is_terrain(terrain));
  CHECK(session.selection() == terrain);
  app.camera().look_at(place + Vec3{0.0f, 40.0f, 30.0f}, place);
  run(app, kFrameFrames);

  // Hovering shows where the brush is without changing anything; holding it raises the ground.
  const float center_x = app.viewport_width() * kViewportCenter;
  const float center_y = app.viewport_height() * kViewportCenter;
  const auto hovered = app.sculpt_at(center_x, center_y, 0.0f);
  REQUIRE(hovered.has_value());
  CHECK(std::fabs(hovered->y) < kTolerance);
  const auto still_flat = session.terrain_hit(terrain, *hovered + Vec3{0.0f, kDropHeight, 0.0f}, {0.0f, -1.0f, 0.0f});
  REQUIRE(still_flat.has_value());
  CHECK(std::fabs(still_flat->y) < kTolerance);  // hovering sculpts nothing
  const auto brushed = app.sculpt_at(center_x, center_y, kBrushSeconds);
  REQUIRE(brushed.has_value());
  const auto peak = session.terrain_hit(terrain, *brushed + Vec3{0.0f, kDropHeight, 0.0f}, {0.0f, -1.0f, 0.0f});
  REQUIRE(peak.has_value());
  CHECK(peak->y > kMinHill);
  // The Flatten brush turns the rounded top into a plateau (something to stand on).
  for (int pass = 0; pass < kFlattenPasses; ++pass) {
    CHECK(session.sculpt_terrain(terrain, *peak, app.terrain_tool().radius, kFullFlatten, SculptMode::Flatten));
  }
  const auto top = session.terrain_hit(terrain, *brushed + Vec3{0.0f, kDropHeight, 0.0f}, {0.0f, -1.0f, 0.0f});
  REQUIRE(top.has_value());
  const auto shoulder =
      session.terrain_hit(terrain, *brushed + Vec3{kShoulder, kDropHeight, 0.0f}, {0.0f, -1.0f, 0.0f});
  REQUIRE(shoulder.has_value());
  CHECK(top->y - shoulder->y < kRestTolerance);  // level around the middle
  run(app, kFrameFrames);                        // the renderer picks the new shape up
  take_picture(app, screenshot("editor_terrain"));

  // A crate dropped over the hill comes to rest on its top, not on the flat ground the terrain
  // started as: the collider follows the sculpted shape.
  const ObjectId ball = session.create_primitive(PrimitiveShape::Cube, *top + Vec3{0.0f, kDropHeight, 0.0f});
  session.document().set_prop(ball, props::kBody, std::string("dynamic"));
  session.refresh_preview();
  app.play();
  REQUIRE(session.play_state() == PlayState::Playing);
  run(app, kFallFrames);
  const ObjectId live_ball = session.scene().find(session.document().node(ball).name);
  REQUIRE(live_ball != kNoObject);
  const Vec3 rest = session.scene().world_transform(live_ball).position;
  CHECK(rest.y > kMinHill * kViewportCenter);  // well above the flat ground
  CHECK(rest.y < top->y + kBallRadius + kRestTolerance);
  app.stop();

  // Undo takes it all back: the crate and its body, then the brush strokes.
  while (session.document().can_undo() && session.is_terrain(terrain)) session.undo();
  session.redo();  // the terrain itself comes back, flat as it was created
  const auto flat = session.terrain_hit(terrain, *brushed + Vec3{0.0f, kDropHeight, 0.0f}, {0.0f, -1.0f, 0.0f});
  REQUIRE(flat.has_value());
  CHECK(std::fabs(flat->y) < kTolerance);
  CHECK(app.console().error_count() == 0);
  std::filesystem::remove_all(game);
}
