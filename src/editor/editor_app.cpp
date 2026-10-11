#include "editor/editor_app.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <rlImGui.h>
#include <ImGuizmo.h>

#include <algorithm>

#include "app/game_project.hpp"
#include "app/user_settings.hpp"
#include "core/config.hpp"
#include "core/log.hpp"
#include "editor/editor_config.hpp"
#include "editor/picking.hpp"
#include "editor/script_files.hpp"
#include "editor/ui/panels.hpp"
#include "editor/ui/style.hpp"
#include "physics/body_config.hpp"
#include "render/renderer.hpp"

namespace redgem {
namespace {

using namespace editor_config;

constexpr const char* kBuildPopup = "build_target";
constexpr float kSmoothingPerStrength = 0.5f;  // Smooth and Flatten: share of the way per second, per unit of strength
constexpr float kBrushLift = 0.05f;            // meters the brush ring floats above the ground
constexpr float kBrushPoleShare = 0.25f;       // height of the brush's center mark, as a share of its radius
constexpr Vector3 kWorldRight{1.0f, 0.0f, 0.0f};
constexpr float kQuarterTurn = 90.0f;          // degrees: lays a circle flat
constexpr int kExitSuccess = 0;
constexpr int kCubeCorners = 8;
constexpr int kCubeEdges = 12;
constexpr int kEdgeEnds = 2;
constexpr int kCornerBitX = 1;
constexpr int kCornerBitY = 2;
constexpr int kCornerBitZ = 4;
constexpr float kFlyAxisFull = 1.0f;
constexpr float kCenter = 0.5f;
constexpr float kMaxDropDistance = 200.0f;  // models dropped on the sky land at the spawn point
constexpr float kRightButtons = 2.0f;  // Build and the Inspector toggle
constexpr float kSplitRatio = 0.5f;  // a split tab gets half of its group
constexpr int kMinTabsToSplit = 2;
constexpr long long kStartupFrames = 2;     // the dock layout exists from the second frame on
constexpr float kPlayControls = 2.0f;       // Play/Stop and Pause, centered together
constexpr float kBothSides = 2.0f;          // padding on both sides of the viewport hint
constexpr const char* kDockspaceId = "EditorDock";
constexpr const char* kUnsavedPopup = "Unsaved changes";
constexpr const char* kExternalChangePopup = "Scene changed on disk";
constexpr const char* kShortcutsPopup = "Keyboard shortcuts";
// Render textures are stored upside down: flip V when showing them.
const ImVec2 kFlippedUvTopLeft(0.0f, 1.0f);
const ImVec2 kFlippedUvBottomRight(1.0f, 0.0f);
constexpr Vector3 kWorldUp{0.0f, 1.0f, 0.0f};

constexpr const char* kShortcuts[][2] = {
    {"Ctrl+P", "Play / Stop"},          {"Esc", "Stop playing"},       {"Ctrl+S", "Save scene"},
    {"Ctrl+Z / Ctrl+Y", "Undo / Redo"}, {"Ctrl+D", "Duplicate"},       {"Del", "Delete"},
    {"W / E / R", "Move / Rotate / Scale"}, {"Hold Ctrl", "Snap while dragging"}, {"F", "Frame selection"},
    {"Right mouse + WASD/QE", "Fly the camera"}, {"Mouse wheel", "Zoom"},
    {"Ctrl+B / Ctrl+Alt+B", "Hide / show the left / right panel"}, {"Ctrl+J", "Hide / show the bottom panel"},
};

::Color rgba(const unsigned char (&c)[4]) { return {c[0], c[1], c[2], c[3]}; }
Vector3 rl(Vec3 v) { return {v.x, v.y, v.z}; }

Matrix model_matrix(const Transform& t) {
  const Matrix scale = MatrixScale(t.scale.x, t.scale.y, t.scale.z);
  const Matrix rotation = QuaternionToMatrix({t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w});
  const Matrix translation = MatrixTranslate(t.position.x, t.position.y, t.position.z);
  return MatrixMultiply(MatrixMultiply(scale, rotation), translation);
}

Bounds unit_bounds() {
  Bounds bounds;
  const Vec3 half = Vec3::one() * config::kDefaultColliderHalfExtent;
  bounds.include(-half);
  bounds.include(half);
  return bounds;
}

// Wireframe of a box given in an object's local space.
void draw_box(const Transform& world, const Bounds& bounds, ::Color color) {
  Vector3 corners[kCubeCorners];
  for (int i = 0; i < kCubeCorners; ++i) {
    const Vec3 local{(i & kCornerBitX) ? bounds.max.x : bounds.min.x, (i & kCornerBitY) ? bounds.max.y : bounds.min.y,
                     (i & kCornerBitZ) ? bounds.max.z : bounds.min.z};
    corners[i] = rl(world.position + rotate(world.rotation, hadamard(local, world.scale)));
  }
  static constexpr int kEdges[kCubeEdges][kEdgeEnds] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3},
                                                       {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
  for (const auto& edge : kEdges) DrawLine3D(corners[edge[0]], corners[edge[1]], color);
}

}  // namespace

struct EditorApp::Viewport {
  RenderTexture2D target{};
  int width = 0;
  int height = 0;
  float screen_x = 0.0f;
  float screen_y = 0.0f;
  bool hovered = false;
  bool focused = false;
  Camera3D camera{};

  void resize(int w, int h) {
    w = std::max(w, kMinViewportSize);
    h = std::max(h, kMinViewportSize);
    if (w == width && h == height && target.id != 0) return;
    if (target.id != 0) UnloadRenderTexture(target);
    target = LoadRenderTexture(w, h);
    width = w;
    height = h;
  }
  ~Viewport() {
    if (target.id != 0) UnloadRenderTexture(target);
  }
};

std::variant<std::unique_ptr<EditorApp>, std::string> EditorApp::create(const std::filesystem::path& game_dir) {
  std::unique_ptr<EditorApp> app(new EditorApp());
  app->console_ = std::make_unique<ConsoleLog>();
  auto opened = EditorSession::open(game_dir);
  if (auto* error = std::get_if<std::string>(&opened)) return *error;
  app->session_ = std::move(std::get<std::unique_ptr<EditorSession>>(opened));
  app->scripts_ = std::make_unique<ScriptWorkspace>(game_dir / config::kScriptsDirName);

  RenderSettings settings;
  settings.width = kWindowWidth;
  settings.height = kWindowHeight;
  settings.title = kWindowTitle;
  settings.show_fps = false;
  settings.resizable = true;
  app->renderer_ = std::make_unique<Renderer>(settings);
  app->renderer_->upload(app->session_->scene_desc());
  app->renderer_->set_images_dir(app->session_->game_dir() / config::kImagesDirName);
  app->viewport_ = std::make_unique<Viewport>();
  app->audio_ = create_raylib_audio();

  rlImGuiSetLoadFontsCallback(ui::load_fonts);
  rlImGuiSetup(true);
  ui::apply_theme();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.IniFilename = nullptr;  // the default layout is rebuilt on every start

  // Start looking through the scene's own camera, like the game would.
  const Scene& scene = app->session_->scene();
  const ObjectId main_camera = scene.main_camera();
  if (main_camera != kNoObject) {
    const Transform world = scene.world_transform(main_camera);
    app->camera_.look_at(world.position, world.position + world.forward());
  }
  app->rescan_files();
  ui::reset_panel_state();  // the hub may open several games in one run
  log::info("editing " + app->session_->document().path().string());
  return app;
}

EditorApp::~EditorApp() {
  session_.reset();
  viewport_.reset();  // GPU resources go before the window
  if (renderer_) rlImGuiShutdown();
  renderer_.reset();
}

bool EditorApp::close_requested() const { return WindowShouldClose(); }

void EditorApp::rescan_files() { files_ = ProjectFiles::scan(session_->game_dir()); }

int EditorApp::run() {
  while (true) {
    // raylib reports the close button for a single frame, so remember it.
    if (close_requested()) request_exit(EditorExit::Quit);
    frame();
    if (quit_confirmed_) break;
  }
  return kExitSuccess;
}

bool EditorApp::editing_blocked() const {
  // No structural edits or undo while a drag or a dialog is in progress.
  return ImGuizmo::IsUsing() || ImGui::IsAnyItemActive() || ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);
}

// --- Commands ------------------------------------------------------------------------------

void EditorApp::play() {
  std::string error;
  // Like an IDE "save all": the game must run the code on screen.
  if (!scripts_->save_all(error)) log::error(error);
  rescan_files();
  if (!session_->play(game_input_, error, audio_.get())) {
    log::error("cannot play: " + error);
    return;
  }
  runtime_scene_generation_ = 0;
  ImGui::SetWindowFocus(kViewportWindow);  // the game gets the keyboard right away
}

void EditorApp::stop() {
  const bool switched_scene = session_->runtime() != nullptr && session_->runtime()->scene_generation() > 0;
  session_->stop();
  audio_->stop_music();
  if (switched_scene) renderer_->upload(session_->scene_desc());  // back to the edited scene's assets
}

void EditorApp::toggle_pause() { session_->pause(session_->play_state() == PlayState::Playing); }

void EditorApp::save() {
  std::string error;
  if (!session_->save(error)) log::error("save failed: " + error);
}

void EditorApp::request_exit(EditorExit action) {
  exit_action_ = action;
  if (session_->play_state() != PlayState::Editing) stop();
  if (session_->document().dirty() || scripts_->any_modified()) {
    close_pending_ = true;
  } else {
    quit_confirmed_ = true;
  }
}

std::optional<std::filesystem::path> EditorApp::build(BuildTarget target) {
  if (session_->document().dirty()) save();
  const std::filesystem::path game = session_->game_dir();
  // "<game>-build" for this system, "<game>-build-windows" for another.
  const std::string suffix = target == host_build_target() ? std::string(kBuildSuffix)
                                                           : std::string(kBuildSuffix) + "-" + build_target_name(target);
  const std::filesystem::path out = game.parent_path() / (game.filename().string() + suffix);
  std::string error;
  const auto executable = build_game_for(game, out, target, error);
  if (!executable) {
    log::error("build failed: " + error);
    return std::nullopt;
  }
  log::info(std::string("built for ") + build_target_name(target) + ": " + executable->string());
  return out;
}

void EditorApp::draw_build_targets() {
  for (const BuildTarget target : {BuildTarget::Linux, BuildTarget::Windows}) {
    const char* label = target == BuildTarget::Windows ? ICON_FA_DESKTOP "  For Windows (.exe)" : ICON_FA_DESKTOP "  For Linux";
    if (ImGui::MenuItem(label)) build(target);
  }
}

Vec3 EditorApp::spawn_point() const { return camera_.position() + camera_.forward() * kSpawnDistance; }

void EditorApp::frame_selection() {
  if (session_->selection() == kNoObject) return;
  Vec3 center;
  float radius = 0.0f;
  object_extent(session_->scene(), session_->scene_desc(), session_->selection(), center, radius);
  camera_.frame(center, radius);
}

ObjectId EditorApp::pick_at(float x, float y) {
  const Ray ray = GetScreenToWorldRayEx({x, y}, viewport_->camera, viewport_->width, viewport_->height);
  const PickRay pick{{ray.position.x, ray.position.y, ray.position.z},
                     normalize(Vec3{ray.direction.x, ray.direction.y, ray.direction.z})};
  const ObjectId hit = pick_object(session_->scene(), session_->scene_desc(), pick);
  session_->select(hit);
  return hit;
}

std::optional<Vec3> EditorApp::sculpt_at(float x, float y, float seconds) {
  const ObjectId terrain = session_->selection();
  const Ray ray = GetScreenToWorldRayEx({x, y}, viewport_->camera, viewport_->width, viewport_->height);
  const auto point = session_->terrain_hit(terrain, {ray.position.x, ray.position.y, ray.position.z},
                                           normalize(Vec3{ray.direction.x, ray.direction.y, ray.direction.z}));
  if (!point || seconds <= 0.0f) return point;
  if (!stroke_started_) {
    session_->document().checkpoint();  // one undo step per stroke
    stroke_started_ = true;
  }
  SculptMode mode = terrain_tool_.mode;
  if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {  // Shift digs with Raise and piles up with Lower
    mode = mode == SculptMode::Raise ? SculptMode::Lower : mode == SculptMode::Lower ? SculptMode::Raise : mode;
  }
  const bool meters = mode == SculptMode::Raise || mode == SculptMode::Lower;
  const float amount = terrain_tool_.strength * seconds * (meters ? 1.0f : kSmoothingPerStrength);
  session_->sculpt_terrain(terrain, *point, terrain_tool_.radius, amount, mode);
  return point;
}

float EditorApp::viewport_width() const { return static_cast<float>(viewport_->width); }
float EditorApp::viewport_height() const { return static_cast<float>(viewport_->height); }

// --- Frame -----------------------------------------------------------------------------------

float EditorApp::frame_seconds() const { return fixed_frame_seconds_.value_or(GetFrameTime()); }

void EditorApp::frame() {
  ++frame_count_;
  if (frame_count_ % kReloadCheckFrames == 0 && session_->reload_if_changed_on_disk()) {
    renderer_->upload(session_->scene_desc());
  }
  if (frame_count_ % kAssetScanFrames == 0) {
    rescan_files();
    scripts_->sync_with_disk();
  }
  handle_shortcuts();
  update_camera();
  game_input_.enabled = viewport_->focused && session_->play_state() == PlayState::Playing;
  game_input_.origin_x = viewport_->screen_x;
  game_input_.origin_y = viewport_->screen_y;
  if (Engine* runtime = session_->runtime()) {
    runtime->set_screen(viewport_->width, viewport_->height,
                        [this](const std::string& text, int size) { return renderer_->text_width(text, size); });
    // A game that locks the pointer gets it while it has the focus; Esc (Stop) gives it back.
    renderer_->set_cursor_locked(game_input_.enabled && runtime->mouse_locked());
    renderer_->set_motion_blur(runtime->motion_blur());
  } else {
    renderer_->set_cursor_locked(false);
    renderer_->set_motion_blur(0.0f);
  }
  session_->tick(frame_seconds());
  render_viewport_texture();
  update_title();

  BeginDrawing();
  ClearBackground(rgba(kViewportBackground));
  rlImGuiBegin();
  ImGuizmo::BeginFrame();
  gizmo_drawn_ = false;
  draw_menu_bar();
  draw_toolbar();
  draw_status_bar();
  draw_dockspace();
  ui::PanelContext context{*session_, *scripts_, files_, spawn_point(), audio_.get()};
  context.terrain = &terrain_tool_;
  if (panels_.hierarchy && ui::draw_hierarchy(context).frame_requested) frame_selection();
  if (panels_.inspector) ui::draw_inspector(context);
  if (panels_.bottom) {
    ui::draw_project(context);
    ui::draw_console(*console_);
  }
  draw_viewport_window();  // before the script tabs: the Scene tab comes first
  const ui::ScriptTabsResult tabs = ui::draw_script_tabs(*scripts_, new_script_dock());
  if (tabs.focused_dock != 0) script_dock_ = tabs.focused_dock;
  if (tabs.split_right) pending_split_ = tabs.split_right;
  if (context.files_changed) rescan_files();

  if (!gizmo_drawn_ && gizmo_dragging_) {
    gizmo_dragging_ = false;  // the drag ended without the gizmo (selection cleared, play started)
    session_->document().drop_checkpoint_if_unchanged();
  }
  draw_dialogs();
  rlImGuiEnd();

  if (screenshot_) {
    rlDrawRenderBatchActive();
    Image image = LoadImageFromScreen();
    std::filesystem::create_directories(screenshot_->parent_path());
    if (!ExportImage(image, screenshot_->string().c_str())) log::error("cannot write " + screenshot_->string());
    UnloadImage(image);
    screenshot_.reset();
  }
  EndDrawing();
}

void EditorApp::draw_dialogs() {
  if (close_pending_ && !ImGui::IsPopupOpen(kUnsavedPopup)) ImGui::OpenPopup(kUnsavedPopup);
  if (ImGui::BeginPopupModal(kUnsavedPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextColored(ui::warning_color(), ICON_FA_TRIANGLE_EXCLAMATION);
    ImGui::SameLine();
    ImGui::Text("Save changes to %s before closing?", session_->game_dir().filename().string().c_str());
    ImGui::Spacing();
    if (ImGui::Button(ICON_FA_FLOPPY_DISK "  Save")) {
      save();
      std::string error;
      if (!scripts_->save_all(error)) log::error(error);
      // Stay open if saving the scene or a script failed.
      quit_confirmed_ = !session_->document().dirty() && !scripts_->any_modified();
      close_pending_ = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Discard")) {
      log::warning("closing without saving");
      quit_confirmed_ = true;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      close_pending_ = false;
      exit_action_ = EditorExit::Quit;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  // One question at a time: closing is asked first.
  if (session_->external_change_pending() && !close_pending_ && !ImGui::IsPopupOpen(kExternalChangePopup)) {
    ImGui::OpenPopup(kExternalChangePopup);
  }
  if (ImGui::BeginPopupModal(kExternalChangePopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("%s changed on disk (Blender export?) while you have unsaved edits.",
                session_->document().path().filename().string().c_str());
    if (ImGui::Button("Reload from disk")) {
      if (session_->reload_from_disk()) renderer_->upload(session_->scene_desc());
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Keep my edits")) {
      session_->keep_my_edits();
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (shortcuts_open_) {
    ImGui::OpenPopup(kShortcutsPopup);
    shortcuts_open_ = false;
  }
  if (ImGui::BeginPopupModal(kShortcutsPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    if (ImGui::BeginTable("shortcuts", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
      for (const auto& [keys, action] : kShortcuts) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(ui::accent_color(), "%s", keys);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(action);
      }
      ImGui::EndTable();
    }
    if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }
}

void EditorApp::update_title() {
  std::string title = std::string(kWindowTitle) + " - " + session_->game_dir().filename().string();
  if (session_->document().dirty()) title += " *";
  if (session_->play_state() != PlayState::Editing) title += " [playing]";
  if (title != title_) {
    title_ = title;
    SetWindowTitle(title_.c_str());
  }
}

void EditorApp::handle_shortcuts() {
  const ImGuiIO& io = ImGui::GetIO();
  if (io.WantTextInput || editing_blocked()) return;
  const bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
  const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
  const bool editing = session_->play_state() == PlayState::Editing;
  const bool alt = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
  if (ctrl && IsKeyPressed(KEY_P)) editing ? play() : stop();
  // Panel toggles, as in VS Code.
  if (ctrl && IsKeyPressed(KEY_B)) (alt ? panels_.inspector : panels_.hierarchy) ^= true;
  if (ctrl && IsKeyPressed(KEY_J)) panels_.bottom ^= true;
  if (!editing) {
    if (IsKeyPressed(KEY_ESCAPE)) stop();
    return;  // the game owns the keyboard while playing
  }
  if (ctrl && IsKeyPressed(KEY_S)) save();
  if (ctrl && IsKeyPressed(KEY_Z)) shift ? session_->redo() : session_->undo();
  if (ctrl && IsKeyPressed(KEY_Y)) session_->redo();
  if (ctrl && IsKeyPressed(KEY_D)) session_->duplicate_selection();
  if (IsKeyPressed(KEY_DELETE)) session_->delete_selection();
  if (ctrl || IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) return;  // WASD flies the camera
  if (IsKeyPressed(KEY_F)) frame_selection();
  if (IsKeyPressed(KEY_W)) gizmo_ = GizmoMode::Translate;
  if (IsKeyPressed(KEY_E)) gizmo_ = GizmoMode::Rotate;
  if (IsKeyPressed(KEY_R)) gizmo_ = GizmoMode::Scale;
}

void EditorApp::update_camera() {
  if (session_->play_state() != PlayState::Editing || !viewport_->hovered) return;
  FlyInput input;
  input.dt = frame_seconds();
  input.zoom = GetMouseWheelMove();
  if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
    const Vector2 delta = GetMouseDelta();
    input.look_x = delta.x;
    input.look_y = delta.y;
    auto axis = [](int negative, int positive) {
      return (IsKeyDown(positive) ? kFlyAxisFull : 0.0f) - (IsKeyDown(negative) ? kFlyAxisFull : 0.0f);
    };
    input.forward = axis(KEY_S, KEY_W);
    input.right = axis(KEY_A, KEY_D);
    input.up = axis(KEY_Q, KEY_E);
    input.fast = IsKeyDown(KEY_LEFT_SHIFT);
  }
  camera_.update(input);
}

void EditorApp::render_viewport_texture() {
  Viewport& view = *viewport_;
  if (view.target.id == 0) return;
  const Scene& scene = session_->scene();
  const bool editing = session_->play_state() == PlayState::Editing;
  if (editing) {
    view.camera.position = rl(camera_.position());
    view.camera.target = rl(camera_.target());
    view.camera.up = kWorldUp;
    view.camera.fovy = camera_.fov_y_degrees();
    view.camera.projection = CAMERA_PERSPECTIVE;
  } else {
    view.camera = renderer_->scene_camera(scene);
  }
  renderer_->set_show_prefabs(editing);  // templates are edited in the editor, hidden in the game
  if (Engine* runtime = session_->runtime(); runtime != nullptr) {
    // The game may have loaded another scene: draw with that scene's meshes and materials.
    if (runtime->scene_generation() != runtime_scene_generation_) {
      runtime_scene_generation_ = runtime->scene_generation();
      renderer_->upload(runtime->scene_desc());
    }
  } else {
    renderer_->sync(session_->scene_desc());  // objects and materials created in the editor
    for (const int mesh : session_->document().take_changed_meshes()) {
      renderer_->refresh_mesh(session_->scene_desc(), mesh);  // sculpted terrain
    }
  }
  renderer_->draw_world(scene, view.camera, &view.target);
  BeginTextureMode(view.target);  // overlays and HUD go on top of the finished image
  if (editing) {
    BeginMode3D(view.camera);
    draw_overlays();
    EndMode3D();
  } else if (Engine* runtime = session_->runtime()) {
    renderer_->draw_hud(runtime->hud());
  }
  EndTextureMode();
}

void EditorApp::draw_overlays() {
  DrawGrid(kGridSlices, kGridSpacing);
  const Scene& scene = session_->scene();
  const SceneDesc& desc = session_->scene_desc();
  for (const ObjectId id : scene.alive_ids()) {
    const GameObject& object = scene.at(id);
    const Transform world = scene.world_transform(id);
    const Bounds bounds = object.mesh != kNoIndex ? desc.meshes[object.mesh].bounds : unit_bounds();
    if (body_config_from_props(object.props).trigger) draw_box(world, bounds, rgba(kTriggerColor));
    if (object.camera) {
      DrawCubeWires(rl(world.position), kIconSize, kIconSize, kIconSize, rgba(kIconColor));
      DrawLine3D(rl(world.position), rl(world.position + world.forward() * kCameraFrustumLength), rgba(kIconColor));
    } else if (object.light) {
      DrawSphereWires(rl(world.position), kIconSize, kIconRings, kIconSlices, rgba(kSunColor));
      DrawLine3D(rl(world.position), rl(world.position + world.forward() * kSunArrowLength), rgba(kSunColor));
    } else if (object.mesh == kNoIndex) {
      DrawCubeWires(rl(world.position), kIconSize, kIconSize, kIconSize, rgba(kIconColor));
    }
  }
  const ObjectId selected = session_->selection();
  if (scene.is_alive(selected)) {
    const GameObject& object = scene.at(selected);
    const Bounds bounds = object.mesh != kNoIndex ? desc.meshes[object.mesh].bounds : unit_bounds();
    draw_box(scene.world_transform(selected), bounds, rgba(kSelectionColor));
  }
  if (brush_point_) {
    // The brush: a ring lying on the ground, and its center.
    DrawCircle3D(rl(*brush_point_ + Vec3::up() * kBrushLift), terrain_tool_.radius, kWorldRight, kQuarterTurn,
                 rgba(kSelectionColor));
    DrawLine3D(rl(*brush_point_), rl(*brush_point_ + Vec3::up() * terrain_tool_.radius * kBrushPoleShare),
               rgba(kSelectionColor));
  }
}

// --- Bars --------------------------------------------------------------------------------------

void EditorApp::draw_menu_bar() {
  if (!ImGui::BeginMainMenuBar()) return;
  const bool editing = session_->play_state() == PlayState::Editing;
  SceneDocument& doc = session_->document();
  ImGui::TextColored(ui::accent_color(), ICON_FA_GEM);
  if (ImGui::BeginMenu("File")) {
    if (ImGui::MenuItem(ICON_FA_FILE_CIRCLE_PLUS "  New Game...")) request_exit(EditorExit::OpenHub);
    if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN "  Open Game...")) request_exit(EditorExit::OpenHub);
    ImGui::Separator();
    if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK "  Save Scene", "Ctrl+S", false, editing)) save();
    if (ImGui::BeginMenu(ICON_FA_BOX_ARCHIVE "  Build Game", editing)) {
      draw_build_targets();
      ImGui::EndMenu();
    }
    if (ImGui::MenuItem(ICON_FA_FOLDER "  Show Game Folder")) open_in_system(session_->game_dir());
    ImGui::Separator();
    if (ImGui::MenuItem(ICON_FA_RIGHT_FROM_BRACKET "  Quit")) request_exit(EditorExit::Quit);
    ImGui::EndMenu();
  }
  if (ImGui::BeginMenu("Edit")) {
    if (ImGui::MenuItem(ICON_FA_ROTATE_LEFT "  Undo", "Ctrl+Z", false, editing && doc.can_undo())) session_->undo();
    if (ImGui::MenuItem(ICON_FA_ROTATE_RIGHT "  Redo", "Ctrl+Y", false, editing && doc.can_redo())) session_->redo();
    ImGui::Separator();
    const bool has_selection = session_->selection() != kNoObject;
    if (ImGui::MenuItem(ICON_FA_CLONE "  Duplicate", "Ctrl+D", false, editing && has_selection)) {
      session_->duplicate_selection();
    }
    if (ImGui::MenuItem(ICON_FA_TRASH "  Delete", "Del", false, editing && has_selection)) session_->delete_selection();
    ImGui::Separator();
    if (ImGui::MenuItem(ICON_FA_EXPAND "  Frame Selection", "F", false, has_selection)) frame_selection();
    ImGui::Separator();
    if (ImGui::BeginMenu(ICON_FA_MICROCHIP "  Graphics Card")) {
      draw_graphics_card_menu();
      ImGui::EndMenu();
    }
    ImGui::EndMenu();
  }
  if (ImGui::BeginMenu("View")) {
    ImGui::MenuItem(ICON_FA_LIST "  Hierarchy (left)", "Ctrl+B", &panels_.hierarchy);
    ImGui::MenuItem(ICON_FA_SLIDERS "  Inspector (right)", "Ctrl+Alt+B", &panels_.inspector);
    ImGui::MenuItem(ICON_FA_TERMINAL "  Project and Console (bottom)", "Ctrl+J", &panels_.bottom);
    ImGui::Separator();
    if (ImGui::MenuItem(ICON_FA_CODE "  Focus on code and scene")) panels_ = {false, false, false};
    if (ImGui::MenuItem(ICON_FA_TABLE_COLUMNS "  Show all panels")) panels_ = {};
    ImGui::EndMenu();
  }
  if (ImGui::BeginMenu("GameObject", editing)) {
    ui::PanelContext context{*session_, *scripts_, files_, spawn_point(), audio_.get()};
    ui::draw_create_menu(context);
    ImGui::EndMenu();
  }
  if (ImGui::BeginMenu("Help")) {
    if (ImGui::MenuItem(ICON_FA_KEYBOARD "  Keyboard Shortcuts")) shortcuts_open_ = true;
    ImGui::EndMenu();
  }
  ImGui::EndMainMenuBar();
}

// Saved for this user (not in the game): which card draws the editor and the games it starts.
void EditorApp::draw_graphics_card_menu() {
  struct Option {
    GpuChoice choice;
    const char* label;
  };
  static constexpr Option kOptions[] = {
      {GpuChoice::Auto, "Automatic (dedicated when there is one)"},
      {GpuChoice::Dedicated, "Dedicated (faster)"},
      {GpuChoice::Integrated, "Integrated (cooler and quieter, much slower)"},
  };
  UserSettings settings = read_user_settings(user_settings_file());
  for (const Option& option : kOptions) {
    if (!ImGui::MenuItem(option.label, nullptr, settings.gpu == option.choice)) continue;
    settings.gpu = option.choice;
    std::string error;
    if (write_user_settings(user_settings_file(), settings, error)) {
      log::info(std::string("graphics card setting: ") + gpu_choice_name(option.choice) +
                " (used the next time the editor or a game starts)");
    } else {
      log::error("cannot save the setting: " + error);
    }
  }
  ImGui::Separator();
  ImGui::TextColored(ui::muted_color(), "Takes effect on the next start.");
}

void EditorApp::draw_toolbar() {
  const ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  const bool open = ImGui::BeginViewportSideBar("##toolbar", ImGui::GetMainViewport(), ImGuiDir_Up, kToolbarHeight, flags);
  ImGui::PopStyleVar();
  ImGui::PopStyleColor();
  if (!open) {
    ImGui::End();
    return;
  }
  const bool editing = session_->play_state() == PlayState::Editing;
  if (ui::icon_button(ICON_FA_TABLE_LIST, "Show / hide the Hierarchy (Ctrl+B)", panels_.hierarchy)) {
    panels_.hierarchy = !panels_.hierarchy;
  }
  ImGui::SameLine(0.0f, kToolbarGroupGap);
  // Left: gizmo tools.
  ImGui::BeginDisabled(!editing);
  if (ui::icon_button(ICON_FA_UP_DOWN_LEFT_RIGHT, "Move (W)", gizmo_ == GizmoMode::Translate)) gizmo_ = GizmoMode::Translate;
  ImGui::SameLine();
  if (ui::icon_button(ICON_FA_ROTATE, "Rotate (E)", gizmo_ == GizmoMode::Rotate)) gizmo_ = GizmoMode::Rotate;
  ImGui::SameLine();
  if (ui::icon_button(ICON_FA_UP_RIGHT_AND_DOWN_LEFT_FROM_CENTER, "Scale (R)", gizmo_ == GizmoMode::Scale)) {
    gizmo_ = GizmoMode::Scale;
  }
  ImGui::SameLine(0.0f, kToolbarGroupGap);
  if (ui::icon_button(gizmo_local_ ? ICON_FA_CUBE : ICON_FA_GLOBE,
                      gizmo_local_ ? "Local axes (click for world)" : "World axes (click for local)")) {
    gizmo_local_ = !gizmo_local_;
  }
  ImGui::SameLine();
  if (ui::icon_button(ICON_FA_MAGNET, "Snap to grid (or hold Ctrl while dragging)", snapping_)) snapping_ = !snapping_;
  ImGui::EndDisabled();

  // Center: play controls.
  const float button = ImGui::GetFrameHeight() * kIconButtonWidth;
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  const float group = button * kPlayControls + spacing;
  ImGui::SameLine((ImGui::GetWindowContentRegionMax().x - group) * kCenter);
  if (editing) {
    if (ui::icon_button(ICON_FA_PLAY, "Play (Ctrl+P)")) play();
  } else {
    ImGui::PushStyleColor(ImGuiCol_Text, ui::accent_color());
    if (ui::icon_button(ICON_FA_STOP, "Stop (Ctrl+P / Esc)", true)) stop();
    ImGui::PopStyleColor();
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(editing);
  if (ui::icon_button(ICON_FA_PAUSE, "Pause", session_->play_state() == PlayState::Paused)) toggle_pause();
  ImGui::EndDisabled();

  // Right: the game.
  const std::string game = session_->game_dir().filename().string();
  const std::string label = std::string(ICON_FA_GAMEPAD "  ") + game + (session_->document().dirty() ? "  " ICON_FA_CIRCLE : "");
  const float width = ImGui::CalcTextSize(label.c_str()).x;
  ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - width - (button + spacing) * kRightButtons -
                  kToolbarGroupGap);
  ImGui::AlignTextToFramePadding();
  ImGui::TextColored(ui::muted_color(), "%s", label.c_str());
  ImGui::SameLine();
  ImGui::BeginDisabled(!editing);
  if (ui::icon_button(ICON_FA_BOX_ARCHIVE, "Build the game: an executable for Linux or Windows")) {
    ImGui::OpenPopup(kBuildPopup);
  }
  if (ImGui::BeginPopup(kBuildPopup)) {
    draw_build_targets();
    ImGui::EndPopup();
  }
  ImGui::EndDisabled();
  ImGui::SameLine(0.0f, kToolbarGroupGap);
  if (ui::icon_button(ICON_FA_TABLE_LIST, "Show / hide the Inspector (Ctrl+Alt+B)", panels_.inspector)) {
    panels_.inspector = !panels_.inspector;
  }
  ImGui::End();
}

void EditorApp::draw_status_bar() {
  const ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ImGui::GetStyle().WindowPadding.x, 0.0f));
  const bool open = ImGui::BeginViewportSideBar("##status", ImGui::GetMainViewport(), ImGuiDir_Down, kStatusBarHeight, flags);
  ImGui::PopStyleVar();
  ImGui::PopStyleColor();
  if (!open) {
    ImGui::End();
    return;
  }
  ImGui::SetCursorPosY((kStatusBarHeight - ImGui::GetTextLineHeight()) * kCenter);
  if (!console_->entries().empty()) {
    const ConsoleEntry& last = console_->entries().back();
    const ImVec4 color = last.level == log::Level::Error     ? ui::error_color()
                         : last.level == log::Level::Warning ? ui::warning_color()
                                                             : ui::muted_color();
    ImGui::TextColored(color, "%s", last.message.c_str());
  }
  const char* state = session_->play_state() == PlayState::Playing  ? ICON_FA_PLAY "  Playing"
                      : session_->play_state() == PlayState::Paused ? ICON_FA_PAUSE "  Paused"
                                                                     : ICON_FA_PEN "  Editing";
  const ObjectId selected = session_->selection();
  char info[kTextBufferSize];
  ImFormatString(info, sizeof(info), "%s    %s %d objects    %s    %d FPS",
                 selected != kNoObject ? session_->document().node(selected).name.c_str() : "",
                 ICON_FA_CUBES, session_->document().node_count(), state, GetFPS());
  ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize(info).x);
  ImGui::TextColored(ui::muted_color(), "%s", info);
  ImGui::End();
}

void EditorApp::draw_dockspace() {
  const ImGuiViewport* main = ImGui::GetMainViewport();
  const ImGuiID dock = ImGui::GetID(kDockspaceId);
  if (!layout_built_) {
    layout_built_ = true;
    ImGui::DockBuilderRemoveNode(dock);
    ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dock, main->WorkSize);
    ImGuiID center = dock;
    const ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, kHierarchyWidth, nullptr, &center);
    const ImGuiID right =
        ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, kInspectorWidth / (1.0f - kHierarchyWidth), nullptr, &center);
    const ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, kBottomHeight, nullptr, &center);
    ImGui::DockBuilderDockWindow(kHierarchyWindow, left);
    ImGui::DockBuilderDockWindow(kInspectorWindow, right);
    ImGui::DockBuilderDockWindow(kProjectWindow, bottom);
    ImGui::DockBuilderDockWindow(kConsoleWindow, bottom);
    ImGui::DockBuilderDockWindow(kViewportWindow, center);
    ImGui::DockBuilderFinish(dock);
    center_dock_ = center;
  }
  ImGui::DockSpaceOverViewport(dock, main);
  apply_pending_split();
}

void EditorApp::split_script_right(const std::string& file_name) { pending_split_ = ui::script_window_name(file_name); }

void EditorApp::apply_pending_split() {
  if (!pending_split_) return;
  const std::string name = *pending_split_;
  pending_split_.reset();
  ImGuiWindow* window = ImGui::FindWindowByName(name.c_str());
  if (window == nullptr || window->DockNode == nullptr) return;
  if (window->DockNode->Windows.Size < kMinTabsToSplit) {
    log::info("drag another tab here first: a single tab has nothing to split from");
    return;
  }
  ImGuiID right = 0;
  ImGuiID left = 0;
  ImGui::DockBuilderSplitNode(window->DockNode->ID, ImGuiDir_Right, kSplitRatio, &right, &left);
  ImGui::DockBuilderDockWindow(name.c_str(), right);
  ImGui::DockBuilderFinish(ImGui::GetID(kDockspaceId));
  script_dock_ = right;
}

unsigned int EditorApp::new_script_dock() const {
  // New scripts open next to the last script used (like a code editor's active group), else next
  // to the Scene.
  if (script_dock_ != 0 && ImGui::DockBuilderGetNode(script_dock_) != nullptr) return script_dock_;
  if (const ImGuiWindow* scene = ImGui::FindWindowByName(kViewportWindow); scene != nullptr && scene->DockId != 0) {
    return scene->DockId;
  }
  return center_dock_;
}

// --- Viewport ----------------------------------------------------------------------------------

void EditorApp::draw_viewport_drop_target(float x, float y) {
  if (!ImGui::BeginDragDropTarget()) return;
  const Vector2 mouse = GetMousePosition();
  const float local_x = mouse.x - x;
  const float local_y = mouse.y - y;
  if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kPayloadModel)) {
    // Drop on the ground under the mouse (y = 0), or in front of the camera when aiming at the sky.
    const Ray ray = GetScreenToWorldRayEx({local_x, local_y}, viewport_->camera, viewport_->width, viewport_->height);
    Vec3 at = spawn_point();
    if (ray.direction.y < 0.0f) {
      const float distance = -ray.position.y / ray.direction.y;
      if (distance < kMaxDropDistance) {
        at = Vec3{ray.position.x, ray.position.y, ray.position.z} +
             Vec3{ray.direction.x, ray.direction.y, ray.direction.z} * distance;
      }
    }
    const std::string model = static_cast<const char*>(payload->Data);
    std::string error;
    if (session_->import_model(session_->game_dir() / config::kModelsDirName / model, at, error) == kNoObject) {
      log::error("cannot import " + model + ": " + error);
    }
  }
  if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kPayloadScript)) {
    const ObjectId target = pick_at(local_x, local_y);
    if (target != kNoObject) {
      session_->attach_script(target, static_cast<const char*>(payload->Data));
    } else {
      log::warning("drop scripts on an object");
    }
  }
  ImGui::EndDragDropTarget();
}

void EditorApp::draw_viewport_window() {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  const bool playing = session_->play_state() != PlayState::Editing;
  if (frame_count_ <= kStartupFrames) ImGui::SetNextWindowFocus();  // the Scene tab starts in front
  ImGui::Begin(kViewportWindow, nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  const ImVec2 size = ImGui::GetContentRegionAvail();
  Viewport& view = *viewport_;
  view.resize(static_cast<int>(size.x), static_cast<int>(size.y));
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  view.screen_x = origin.x;
  view.screen_y = origin.y;
  const ImVec2 image_size(static_cast<float>(view.width), static_cast<float>(view.height));
  ImGui::Image(ImTextureRef(static_cast<ImTextureID>(view.target.texture.id)), image_size, kFlippedUvTopLeft,
               kFlippedUvBottomRight);
  if (!playing) draw_viewport_drop_target(origin.x, origin.y);
  view.hovered = ImGui::IsWindowHovered();
  view.focused = ImGui::IsWindowFocused();
  if (view.hovered && IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) ImGui::SetWindowFocus();

  // A hint in the bottom-left corner, on a dark pill so it reads over any scene (and stays clear
  // of the game's own HUD at the top).
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const char* hint = !playing      ? ICON_FA_COMPUTER_MOUSE "  Right mouse + WASD to fly    F frames the selection"
                     : view.focused ? ICON_FA_GAMEPAD "  The game has the keyboard    Esc stops"
                                    : ICON_FA_GAMEPAD "  Click the game to give it the keyboard";
  const ImVec2 hint_size = ImGui::CalcTextSize(hint);
  const ImVec2 pill_min(origin.x + kOverlayMargin, origin.y + image_size.y - kOverlayMargin - hint_size.y -
                                                       kOverlayPadding * kBothSides);
  const ImVec2 pill_max(pill_min.x + hint_size.x + kOverlayPadding * kBothSides, pill_min.y + hint_size.y + kOverlayPadding * kBothSides);
  draw->AddRectFilled(pill_min, pill_max, ImGui::GetColorU32(ImGuiCol_WindowBg, kOverlayAlpha), kOverlayRounding);
  draw->AddText(ImVec2(pill_min.x + kOverlayPadding, pill_min.y + kOverlayPadding),
                ImGui::GetColorU32(playing ? ui::accent_color() : ui::muted_color()), hint);
  if (playing) {
    const ImVec2 end(origin.x + image_size.x, origin.y + image_size.y);
    draw->AddRect(origin, end, ImGui::GetColorU32(ui::accent_color()), 0.0f, 0, kPlayBorderThickness);
  }

  // Sculpting takes the left button over from the gizmo and from picking.
  const bool sculpting = !playing && terrain_tool_.sculpting && session_->is_terrain(session_->selection());
  brush_point_.reset();
  if (sculpting) {
    const Vector2 mouse = GetMousePosition();
    const bool brushing = view.hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left);
    if (view.hovered) {
      brush_point_ = sculpt_at(mouse.x - origin.x, mouse.y - origin.y, brushing ? frame_seconds() : 0.0f);
    }
    if (!brushing && stroke_started_) {
      stroke_started_ = false;
      session_->document().drop_checkpoint_if_unchanged();
    }
  } else if (!playing) {
    stroke_started_ = false;
    if (session_->selection() != kNoObject) draw_gizmo(origin.x, origin.y, image_size.x, image_size.y);
    if (view.hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing()) {
      const Vector2 mouse = GetMousePosition();
      pick_at(mouse.x - origin.x, mouse.y - origin.y);
    }
  }
  ImGui::End();
  ImGui::PopStyleVar();
}

void EditorApp::draw_gizmo(float x, float y, float width, float height) {
  const ObjectId selected = session_->selection();
  const Scene& scene = session_->scene();
  if (!scene.is_alive(selected) || width <= 0.0f || height <= 0.0f) return;
  ImGuizmo::SetOrthographic(false);
  ImGuizmo::SetDrawlist();
  ImGuizmo::SetRect(x, y, width, height);
  const float16 view = MatrixToFloatV(GetCameraMatrix(viewport_->camera));
  // Same projection as raylib's BeginMode3D.
  const float16 projection = MatrixToFloatV(MatrixPerspective(viewport_->camera.fovy * DEG2RAD, width / height,
                                                              rlGetCullDistanceNear(), rlGetCullDistanceFar()));
  float16 model = MatrixToFloatV(model_matrix(scene.world_transform(selected)));
  const ImGuizmo::OPERATION operation = gizmo_ == GizmoMode::Translate ? ImGuizmo::TRANSLATE
                                        : gizmo_ == GizmoMode::Rotate  ? ImGuizmo::ROTATE
                                                                       : ImGuizmo::SCALE;
  // Scaling always works on the object's own axes.
  const ImGuizmo::MODE mode = gizmo_local_ || gizmo_ == GizmoMode::Scale ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
  const bool snap = snapping_ || ImGui::GetIO().KeyCtrl;
  const float step = gizmo_ == GizmoMode::Translate ? kTranslateSnap
                     : gizmo_ == GizmoMode::Rotate  ? kRotateSnapDegrees
                                                    : kScaleSnap;
  const float snap_values[] = {step, step, step};
  ImGuizmo::Manipulate(view.v, projection.v, operation, mode, model.v, nullptr, snap ? snap_values : nullptr);

  gizmo_drawn_ = true;
  const bool using_gizmo = ImGuizmo::IsUsing();
  if (using_gizmo && !gizmo_dragging_) session_->document().checkpoint();  // one undo step per drag
  if (!using_gizmo && gizmo_dragging_) session_->document().drop_checkpoint_if_unchanged();
  gizmo_dragging_ = using_gizmo;
  if (!using_gizmo) return;
  const Transform world = transform_from_matrix(model.v);
  const NodeDesc& node = session_->document().node(selected);
  const Transform local = node.parent == kNoIndex ? world : relative_to(scene.world_transform(node.parent), world);
  session_->document().set_local(selected, local, false);
  session_->refresh_preview();
}

}  // namespace redgem
