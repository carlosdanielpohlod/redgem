#include "editor/editor_app.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <ImGuizmo.h>

#include <algorithm>

#include "app/user_settings.hpp"
#include "core/config.hpp"
#include "scene/scene_config.hpp"
#include "core/log.hpp"
#include "core/platform.hpp"
#include "editor/editor_config.hpp"
#include "editor/editor_logic.hpp"
#include "editor/picking.hpp"
#include "editor/script_files.hpp"
#include "editor/ui/interface.hpp"
#include "editor/ui/panels.hpp"
#include "editor/ui/style.hpp"
#include "physics/body_config.hpp"
#include "app/frame_painter.hpp"
#include "render/renderer.hpp"
#include "render/window.hpp"

namespace redgem {
namespace {

using namespace editor_config;

constexpr float kBrushLift = 0.05f;       // meters the brush ring floats above the ground
constexpr float kBrushPoleShare = 0.25f;  // height of the brush's center mark, as a share of its radius
constexpr Vector3 kWorldRight{1.0f, 0.0f, 0.0f};
constexpr Vector3 kWorldUp{0.0f, 1.0f, 0.0f};
constexpr float kQuarterTurn = 90.0f;  // degrees: lays a circle flat
constexpr int kExitSuccess = 0;        // process exit code of a normal run
constexpr int kBoxEdges = 12;          // of a wireframe box
constexpr int kEdgeEnds = 2;
constexpr float kRightButtons = 2.0f;    // Build and the Inspector toggle
constexpr float kSplitRatio = 0.5f;      // a split tab gets half of its group
constexpr int kMinTabsToSplit = 2;       // a tab needs a neighbour to split away from
constexpr long long kStartupFrames = 2;  // the dock layout exists from the second frame on
constexpr float kPlayControls = 2.0f;    // Play/Stop and Pause, centered together
constexpr float kNoMotionBlur = 0.0f;    // strength of the blur while no game runs
constexpr int kShortcutColumns = 2;      // keys, and what they do
constexpr const char* kDockspaceId = "EditorDock";
constexpr const char* kToolbarId = "##toolbar";
constexpr const char* kStatusBarId = "##status";
constexpr const char* kShortcutsTableId = "shortcuts";
constexpr const char* kAnyPopup = "";  // with ImGuiPopupFlags_AnyPopupId: whichever popup is open
constexpr const char* kBuildPopup = "build_target";
constexpr const char* kNoBuilds = "No way of building was given to this editor.";
constexpr const char* kUnsavedPopup = "Unsaved changes";
constexpr const char* kExternalChangePopup = "Scene changed on disk";
constexpr const char* kShortcutsPopup = "Keyboard shortcuts";
// Render textures are stored upside down: flip V when showing them.
constexpr ImVec2 kFlippedUvTopLeft{0.0f, 1.0f};
constexpr ImVec2 kFlippedUvBottomRight{1.0f, 0.0f};
// Which corners (see editor_logic::box_corners) each edge of a box joins.
constexpr int kBoxEdgeCorners[kBoxEdges][kEdgeEnds] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3},
                                                       {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};

constexpr const char* kShortcuts[][kShortcutColumns] = {
    {"Ctrl+P", "Play / Stop"},
    {"Esc", "Stop playing"},
    {kKeysSave, "Save scene"},
    {"Ctrl+Z / Ctrl+Y", "Undo / Redo"},
    {kKeysDuplicate, "Duplicate"},
    {kKeysDelete, "Delete"},
    {"W / E / R", "Move / Rotate / Scale"},
    {"Hold Ctrl", "Snap while dragging"},
    {kKeysFrame, "Frame selection"},
    {"Right mouse + WASD/QE", "Fly the camera"},
    {"Mouse wheel", "Zoom"},
    {"Ctrl+B / Ctrl+Alt+B", "Hide / show the left / right panel"},
    {kKeysBottomPanel, "Hide / show the bottom panel"},
};

// The Scene view's hints, in ViewportHint's order.
constexpr const char* kViewportHints[] = {
    ICON_FA_COMPUTER_MOUSE "  Right mouse + WASD to fly    F frames the selection",
    ICON_FA_MOUNTAIN "  Terrain: turn Sculpt on in the Inspector to shape it",
    ICON_FA_MOUNTAIN "  Sculpting: drag to shape the ground    Shift reverses    brush in the Inspector",
    ICON_FA_GAMEPAD "  The game has the keyboard    Esc stops",
    ICON_FA_GAMEPAD "  Click the game to give it the keyboard",
};

// Saved for this user (not in the game): which card draws the editor and the games it starts.
struct GpuOption {
  GpuChoice choice;
  const char* label;
};
constexpr GpuOption kGpuOptions[] = {
    {GpuChoice::Auto, "Automatic (dedicated when there is one)"},
    {GpuChoice::Dedicated, "Dedicated (faster)"},
    {GpuChoice::Integrated, "Integrated (cooler and quieter, much slower)"},
};

// Where each channel is in a color given as four numbers.
enum ColorChannel { kRed, kGreen, kBlue, kAlpha };

// What the gizmo can do: the key that picks each mode and its toolbar button.
struct GizmoTool {
  GizmoMode mode;
  int key;
  const char* icon;
  const char* tooltip;
};
constexpr GizmoTool kGizmoTools[] = {
    {GizmoMode::Translate, KEY_W, ICON_FA_UP_DOWN_LEFT_RIGHT, "Move (W)"},
    {GizmoMode::Rotate, KEY_E, ICON_FA_ROTATE, "Rotate (E)"},
    {GizmoMode::Scale, KEY_R, ICON_FA_UP_RIGHT_AND_DOWN_LEFT_FROM_CENTER, "Scale (R)"},
};

::Color rgba(const unsigned char (&c)[config::kChannelsPerColor]) { return {c[kRed], c[kGreen], c[kBlue], c[kAlpha]}; }
Vector3 rl(Vec3 v) { return {v.x, v.y, v.z}; }
Vec3 vec(Vector3 v) { return {v.x, v.y, v.z}; }

bool ctrl_down() { return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL); }
bool shift_down() { return IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT); }

// The fly camera's keys, as the keyboard has them now.
FlyKeys fly_keys() {
  FlyKeys keys;
  keys.forward = IsKeyDown(KEY_W);
  keys.back = IsKeyDown(KEY_S);
  keys.left = IsKeyDown(KEY_A);
  keys.right = IsKeyDown(KEY_D);
  keys.up = IsKeyDown(KEY_E);
  keys.down = IsKeyDown(KEY_Q);
  keys.left_shift = IsKeyDown(KEY_LEFT_SHIFT);
  keys.right_shift = IsKeyDown(KEY_RIGHT_SHIFT);
  return keys;
}
bool alt_down() { return IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT); }

Matrix model_matrix(const Transform& t) {
  const Matrix scale = MatrixScale(t.scale.x, t.scale.y, t.scale.z);
  const Matrix rotation = QuaternionToMatrix({t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w});
  const Matrix translation = MatrixTranslate(t.position.x, t.position.y, t.position.z);
  return MatrixMultiply(MatrixMultiply(scale, rotation), translation);
}

// The box drawn around an object: its mesh's, or a small one for objects without a mesh.
Bounds bounds_of(const GameObject& object, const SceneDesc& desc) {
  return object.mesh != kNoIndex ? desc.meshes[object.mesh].bounds : default_collider_bounds();
}

// Wireframe of a box given in an object's local space.
void draw_box(const Transform& world, const Bounds& bounds, ::Color color) {
  const auto corners = editor_logic::box_corners(world, bounds);
  for (const auto& edge : kBoxEdgeCorners) DrawLine3D(rl(corners[edge[0]]), rl(corners[edge[1]]), color);
}

WindowSettings window_settings(GpuChoice gpu) {
  WindowSettings settings;
  settings.width = kWindowWidth;
  settings.height = kWindowHeight;
  settings.title = kWindowTitle;
  settings.resizable = true;
  settings.gpu = gpu;
  return settings;
}

// Start looking through the scene's own camera, like the game would.
void look_through_main_camera(EditorCamera& camera, const Scene& scene) {
  const ObjectId main_camera = scene.main_camera();
  if (main_camera == kNoObject) return;
  const Transform world = scene.world_transform(main_camera);
  camera.look_at(world.position, world.position + world.forward());
}

// A bar across the top or the bottom of the window, in the menu bar's color. ImGui::End() closes
// it whatever this returns.
bool begin_bar(const char* id, ImGuiDir side, float height) {
  const ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg));
  const bool open = ImGui::BeginViewportSideBar(id, ImGui::GetMainViewport(), side, height, flags);
  ImGui::PopStyleColor();
  return open;
}

// The strip under the menu bar. Like begin_bar, ImGui::End() closes it whatever this returns.
bool begin_toolbar() {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  const bool open = begin_bar(kToolbarId, ImGuiDir_Up, kToolbarHeight);
  ImGui::PopStyleVar();
  return open;
}

bool begin_status_bar() {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ImGui::GetStyle().WindowPadding.x, 0.0f));
  const bool open = begin_bar(kStatusBarId, ImGuiDir_Down, kStatusBarHeight);
  ImGui::PopStyleVar();
  return open;
}

// An empty dock space as big as the window, ready to be split.
void start_layout(ImGuiID dock) {
  ImGui::DockBuilderRemoveNode(dock);
  ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(dock, ImGui::GetMainViewport()->WorkSize);
}

// Moves a tab into a new group on the right half of its own. Returns that group's dock.
ImGuiID split_to_the_right(const ImGuiDockNode& group, const std::string& window_name) {
  ImGuiID right = 0;
  ImGuiID left = 0;
  ImGui::DockBuilderSplitNode(group.ID, ImGuiDir_Right, kSplitRatio, &right, &left);
  ImGui::DockBuilderDockWindow(window_name.c_str(), right);
  ImGui::DockBuilderFinish(ImGui::GetID(kDockspaceId));
  return right;
}

void draw_shortcuts_table() {
  if (!ImGui::BeginTable(kShortcutsTableId, kShortcutColumns, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
    return;
  for (const auto& [keys, action] : kShortcuts) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextColored(ui::accent_color(), "%s", keys);
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(action);
  }
  ImGui::EndTable();
}

// What a picture of the scene would not show of a camera, a light or an empty object: a small
// wire shape where it is, and for the first two a line along where it points.
void draw_camera_mark(const Transform& world) {
  DrawCubeWires(rl(world.position), kIconSize, kIconSize, kIconSize, rgba(kIconColor));
  DrawLine3D(rl(world.position), rl(world.position + world.forward() * kCameraFrustumLength), rgba(kIconColor));
}

void draw_light_mark(const Transform& world) {
  DrawSphereWires(rl(world.position), kIconSize, kIconRings, kIconSlices, rgba(kSunColor));
  DrawLine3D(rl(world.position), rl(world.position + world.forward() * kSunArrowLength), rgba(kSunColor));
}

void draw_mark(const GameObject& object, const Transform& world) {
  if (object.camera) {
    draw_camera_mark(world);
  } else if (object.light) {
    draw_light_mark(world);
  } else if (object.mesh == kNoIndex) {
    DrawCubeWires(rl(world.position), kIconSize, kIconSize, kIconSize, rgba(kIconColor));
  }
}

float icon_button_width() { return ImGui::GetFrameHeight() * kIconButtonWidth; }

ImVec4 level_color(log::Level level) {
  if (level == log::Level::Error) return ui::error_color();
  return level == log::Level::Warning ? ui::warning_color() : ui::muted_color();
}

const char* play_state_label(PlayState state) {
  if (state == PlayState::Playing) return ICON_FA_PLAY "  Playing";
  return state == PlayState::Paused ? ICON_FA_PAUSE "  Paused" : ICON_FA_PEN "  Editing";
}

ImGuizmo::OPERATION gizmo_operation(GizmoMode mode) {
  if (mode == GizmoMode::Translate) return ImGuizmo::TRANSLATE;
  return mode == GizmoMode::Rotate ? ImGuizmo::ROTATE : ImGuizmo::SCALE;
}

float snap_step(GizmoMode mode) {
  if (mode == GizmoMode::Translate) return kTranslateSnap;
  return mode == GizmoMode::Rotate ? kRotateSnapDegrees : kScaleSnap;
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
  // The mouse in the view's own pixels.
  Vector2 mouse() const {
    const Vector2 in_window = GetMousePosition();
    return {in_window.x - screen_x, in_window.y - screen_y};
  }
  // The ray into the scene through a point of the view.
  Ray ray_at(float x, float y) const { return GetScreenToWorldRayEx({x, y}, camera, width, height); }
  ~Viewport() {
    if (target.id != 0) UnloadRenderTexture(target);
  }
};

// Shows the running game in the Scene view, as a window of its own would: the view is the
// game's screen, drawn with the editor's renderer into the view's picture.
class EditorApp::GameView final : public Presenter {
 public:
  GameView(Window& window, Renderer& renderer, Viewport& view, const GatedInputSource& input)
      : window_(window), renderer_(renderer), view_(view), input_(input), painter_(renderer) {
    // The game starts from the scene being edited, which the renderer already holds.
    painter_.assume_uploaded(kFirstSceneGeneration);
  }

  SurfaceSize surface_size() const override { return {view_.width, view_.height}; }
  int text_width(const std::string& text, int size) const override { return renderer_.text_width(text, size); }
  // A game that locks the pointer gets it while it has the focus; Esc (Stop) gives it back.
  void capture_cursor(bool wanted) override { window_.set_cursor_locked(input_.enabled && wanted); }
  void present(const FrameView& view) override {
    if (view_.target.id == 0) return;   // the view was not laid out yet: there is nothing to draw into
    renderer_.set_show_prefabs(false);  // templates are edited in the editor, hidden in the game
    view_.camera = renderer_.scene_camera(view.scene);
    painter_.paint(view, &view_.target);
  }
  // The view is there for as long as the editor is: closing the window is the editor's business.
  bool open() const override { return true; }
  float frame_seconds() const override { return window_.frame_seconds(); }

  // Whether the game loaded another scene, whose assets took the place of the edited scene's.
  bool switched_scene() const { return painter_.uploaded_generation() != kFirstSceneGeneration; }

 private:
  Window& window_;
  Renderer& renderer_;
  Viewport& view_;
  const GatedInputSource& input_;
  FramePainter painter_;
};

std::variant<std::unique_ptr<EditorApp>, std::string> EditorApp::create(const std::filesystem::path& game_dir,
                                                                        EditorOptions options) {
  std::unique_ptr<EditorApp> app(new EditorApp());
  app->builds_ = std::move(options.builds);
  app->console_ = std::make_unique<ConsoleLog>();
  auto opened = EditorSession::open(game_dir);
  if (auto* error = std::get_if<std::string>(&opened)) return *error;
  app->session_ = std::move(std::get<std::unique_ptr<EditorSession>>(opened));
  app->scripts_ = std::make_unique<ScriptWorkspace>(game_dir / config::kScriptsDirName);
  app->open_window(options.gpu, std::move(options.clock));
  log::info("editing " + app->session_->document().path().string());
  return app;
}

// The window, and what draws and sounds in it, for the game just opened.
void EditorApp::open_window(GpuChoice gpu, FrameClock clock) {
  window_ = std::make_unique<Window>(window_settings(gpu));
  clock_ = clock ? std::move(clock) : [this] { return window_->frame_seconds(); };
  renderer_ = std::make_unique<Renderer>(*window_);
  renderer_->upload(session_->scene_desc());
  renderer_->set_images_dir(session_->game_dir() / config::kImagesDirName);
  viewport_ = std::make_unique<Viewport>();
  audio_ = create_raylib_audio();
  interface_ = std::make_unique<ui::Interface>(*window_, ui::Interface::Layout::Dockable);
  look_through_main_camera(camera_, session_->scene());
  rescan_files();
}

EditorApp::~EditorApp() {
  session_.reset();  // the running game, if any, goes before who shows it
  game_view_.reset();
  viewport_.reset();  // GPU resources go before the window
  interface_.reset();
  renderer_.reset();
  window_.reset();
}

bool EditorApp::close_requested() const { return window_->close_requested(); }

void EditorApp::rescan_files() { files_ = ProjectFiles::scan(session_->game_dir()); }

int EditorApp::run() {
  while (!finished()) {
    // raylib reports the close button for a single frame, so remember it.
    if (close_requested()) request_exit(EditorExit::Quit);
    frame();
  }
  return kExitSuccess;
}

bool EditorApp::editing_blocked() const {
  // No structural edits or undo while a drag or a dialog is in progress.
  return ImGuizmo::IsUsing() || ImGui::IsAnyItemActive() || ImGui::IsPopupOpen(kAnyPopup, ImGuiPopupFlags_AnyPopupId);
}

// --- Commands ------------------------------------------------------------------------------

void EditorApp::save_scripts() {
  std::string error;
  if (!scripts_->save_all(error)) log::error(error);
}

void EditorApp::play() {
  save_scripts();  // like an IDE "save all": the game must run the code on screen
  rescan_files();
  std::string error;
  game_view_ = std::make_unique<GameView>(*window_, *renderer_, *viewport_, game_input_);
  if (!session_->play({game_input_, audio_.get(), game_view_.get()}, error)) {
    log::error("cannot play: " + error);
    game_view_.reset();
    return;
  }
  ImGui::SetWindowFocus(kViewportWindow);  // the game gets the keyboard right away
}

void EditorApp::stop() {
  session_->stop();
  retire_game_view();
}

void EditorApp::retire_game_view() {
  if (!game_view_) return;
  if (game_view_->switched_scene()) renderer_->upload(session_->scene_desc());  // back to the edited scene's assets
  game_view_.reset();
}

void EditorApp::toggle_pause() { session_->pause(session_->play_state() == PlayState::Playing); }

bool EditorApp::editing() const { return session_->play_state() == PlayState::Editing; }

ui::PanelContext EditorApp::panel_context() {
  ui::PanelContext context{*session_, *scripts_, files_, panel_state_, spawn_point(), audio_.get()};
  context.terrain = &terrain_tool_;
  return context;
}

void EditorApp::save() {
  std::string error;
  if (!session_->save(error)) log::error("save failed: " + error);
}

void EditorApp::request_exit(EditorExit action) {
  exit_action_ = action;
  if (!editing()) stop();
  if (session_->document().dirty() || scripts_->any_modified()) {
    close_pending_ = true;
  } else {
    quit_confirmed_ = true;
  }
}

std::optional<std::filesystem::path> EditorApp::build(const BuildChoice& choice) {
  if (session_->document().dirty()) save();
  const std::filesystem::path game = session_->game_dir();
  std::filesystem::path out = game.parent_path() / editor_logic::build_folder_name(game.filename().string(),
                                                                                   choice.name, choice.for_this_system);
  std::string error;
  const auto executable = choice.build(game, out, error);
  if (!executable) {
    log::error("build failed: " + error);
    return std::nullopt;
  }
  log::info("built for " + choice.name + ": " + executable->string());
  return out;
}

std::optional<std::filesystem::path> EditorApp::build() {
  const auto own =
      std::find_if(builds_.begin(), builds_.end(), [](const BuildChoice& choice) { return choice.for_this_system; });
  if (own != builds_.end()) return build(*own);
  log::error("build failed: this editor was not given a way to build for this system");
  return std::nullopt;
}

void EditorApp::draw_build_targets() {
  if (builds_.empty()) ImGui::TextColored(ui::muted_color(), "%s", kNoBuilds);
  for (const BuildChoice& choice : builds_) {
    if (ImGui::MenuItem(choice.label.c_str())) build(choice);
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
  const Ray ray = viewport_->ray_at(x, y);
  const PickRay pick{vec(ray.position), normalize(vec(ray.direction))};
  const ObjectId hit = pick_object(session_->scene(), session_->scene_desc(), pick);
  session_->select(hit);
  return hit;
}

std::optional<Vec3> EditorApp::sculpt_at(float x, float y, float seconds) {
  const ObjectId terrain = session_->selection();
  const Ray ray = viewport_->ray_at(x, y);
  const auto point = session_->terrain_hit(terrain, vec(ray.position), normalize(vec(ray.direction)));
  if (!point || seconds <= 0.0f) return point;
  if (!stroke_started_) {
    session_->document().checkpoint();  // one undo step per stroke
    stroke_started_ = true;
  }
  // Shift digs with Raise and piles up with Lower.
  const SculptMode mode = shift_down() ? editor_logic::reversed(terrain_tool_.mode) : terrain_tool_.mode;
  const float amount = editor_logic::brush_amount(mode, terrain_tool_.strength, seconds);
  session_->sculpt_terrain(terrain, *point, terrain_tool_.radius, amount, mode);
  return point;
}

float EditorApp::viewport_width() const { return static_cast<float>(viewport_->width); }
float EditorApp::viewport_height() const { return static_cast<float>(viewport_->height); }

// --- Frame -----------------------------------------------------------------------------------

void EditorApp::frame() {
  ++frame_count_;
  frame_seconds_ = clock_();
  update_before_drawing();
  window_->begin_frame();
  ClearBackground(rgba(kViewportBackground));
  draw_interface();
  window_->end_frame();
}

void EditorApp::update_before_drawing() {
  check_files_on_disk();
  take_dropped_files();
  handle_shortcuts();
  update_camera();
  feed_game();
  session_->tick(frame_seconds());
  if (session_->runtime() == nullptr) retire_game_view();  // the game quit by itself
  render_viewport_texture();
  update_title();
}

void EditorApp::draw_interface() {
  interface_->begin_frame();
  ImGuizmo::BeginFrame();
  gizmo_drawn_ = false;
  draw_menu_bar();
  draw_toolbar();
  draw_status_bar();
  draw_dockspace();
  draw_panels();
  end_abandoned_gizmo_drag();
  draw_dialogs();
  interface_->end_frame();
}

// A drag that ended without the gizmo being drawn (the selection was cleared, play started).
void EditorApp::end_abandoned_gizmo_drag() {
  if (gizmo_drawn_ || !gizmo_dragging_) return;
  gizmo_dragging_ = false;
  session_->document().drop_checkpoint_if_unchanged();
}

void EditorApp::take_dropped_files() {
  if (!IsFileDropped()) return;
  const FilePathList dropped = LoadDroppedFiles();
  const std::vector<std::filesystem::path> files(dropped.paths, dropped.paths + dropped.count);
  UnloadDroppedFiles(dropped);
  add_files(files);
}

void EditorApp::add_files(const std::vector<std::filesystem::path>& files) {
  report_drop(add_dropped_files(session_->game_dir(), files));
  rescan_files();
}

void EditorApp::report_drop(const DropReport& report) {
  for (const std::string& model : report.models) log::info("added " + model + ": drag it from Models into the scene");
  for (const std::string& sound : report.sounds) log::info("added " + sound + " to the game's sounds");
  for (const std::string& refused : report.refused) log::warning("not added: " + refused);
  if (!report.models.empty()) panel_state_.project.folder = ui::ProjectFolder::Models;
  if (report.models.empty() && !report.sounds.empty()) panel_state_.project.folder = ui::ProjectFolder::Sounds;
}

void EditorApp::check_files_on_disk() {
  if (frame_count_ % kReloadCheckFrames == 0 && session_->reload_if_changed_on_disk()) {
    renderer_->upload(session_->scene_desc());
  }
  if (frame_count_ % kAssetScanFrames == 0) {
    rescan_files();
    scripts_->sync_with_disk();
  }
}

// The running game reads its keys and mouse through the Scene view, while the view has the focus.
void EditorApp::feed_game() {
  game_input_.enabled = viewport_->focused && session_->play_state() == PlayState::Playing;
  game_input_.origin_x = viewport_->screen_x;
  game_input_.origin_y = viewport_->screen_y;
  // Nobody playing asks for the pointer: it goes back to the person at the machine.
  if (!game_input_.enabled) window_->set_cursor_locked(false);
}

void EditorApp::draw_panels() {
  ui::PanelContext context = panel_context();
  draw_side_panels(context);
  draw_viewport_window();  // before the script tabs: the Scene tab comes first
  draw_script_tabs();
  if (context.files_changed) rescan_files();
}

void EditorApp::draw_side_panels(ui::PanelContext& context) {
  if (panels_.hierarchy && ui::draw_hierarchy(context).frame_requested) frame_selection();
  if (panels_.inspector) ui::draw_inspector(context);
  // Whatever is selected, and whether or not the Inspector is shown: a drag is over once its
  // widget is let go.
  panel_state_.inspector.edits.end_frame(ImGui::IsAnyItemActive());
  if (!panels_.bottom) return;
  ui::draw_project(context);
  ui::draw_console(*console_, panel_state_.console);
}

void EditorApp::draw_script_tabs() {
  const ui::ScriptTabsResult tabs = ui::draw_script_tabs(*scripts_, panel_state_.script_tabs, new_script_dock());
  if (tabs.focused_dock != 0) script_dock_ = tabs.focused_dock;
  if (tabs.split_right) pending_split_ = tabs.split_right;
}

void EditorApp::draw_dialogs() {
  draw_unsaved_dialog();
  draw_external_change_dialog();  // one question at a time: closing is asked first
  draw_shortcuts_dialog();
}

void EditorApp::draw_unsaved_dialog() {
  if (close_pending_ && !ImGui::IsPopupOpen(kUnsavedPopup)) ImGui::OpenPopup(kUnsavedPopup);
  if (!ImGui::BeginPopupModal(kUnsavedPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
  ImGui::TextColored(ui::warning_color(), ICON_FA_TRIANGLE_EXCLAMATION);
  ImGui::SameLine();
  ImGui::Text("Save changes to %s before closing?", session_->game_dir().filename().string().c_str());
  ImGui::Spacing();
  draw_unsaved_choices();
  ImGui::EndPopup();
}

void EditorApp::draw_unsaved_choices() {
  if (ImGui::Button(ui::kSaveLabel)) save_and_leave();
  ImGui::SameLine();
  if (ImGui::Button(ui::kDiscardLabel)) leave_without_saving();
  ImGui::SameLine();
  if (ImGui::Button(ui::kCancelLabel)) cancel_leaving();
}

void EditorApp::save_and_leave() {
  save();
  save_scripts();
  // Stay open if saving the scene or a script failed.
  quit_confirmed_ = !session_->document().dirty() && !scripts_->any_modified();
  close_pending_ = false;
  ImGui::CloseCurrentPopup();
}

void EditorApp::leave_without_saving() {
  log::warning("closing without saving");
  quit_confirmed_ = true;
  ImGui::CloseCurrentPopup();
}

void EditorApp::cancel_leaving() {
  close_pending_ = false;
  exit_action_ = EditorExit::Quit;
  ImGui::CloseCurrentPopup();
}

void EditorApp::draw_external_change_dialog() {
  if (session_->external_change_pending() && !close_pending_ && !ImGui::IsPopupOpen(kExternalChangePopup)) {
    ImGui::OpenPopup(kExternalChangePopup);
  }
  if (!ImGui::BeginPopupModal(kExternalChangePopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
  ImGui::Text("%s changed on disk (Blender export?) while you have unsaved edits.",
              session_->document().path().filename().string().c_str());
  draw_external_change_choices();
  ImGui::EndPopup();
}

void EditorApp::draw_external_change_choices() {
  if (ImGui::Button("Reload from disk")) {
    if (session_->reload_from_disk()) renderer_->upload(session_->scene_desc());
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (ImGui::Button("Keep my edits")) {
    session_->keep_my_edits();
    ImGui::CloseCurrentPopup();
  }
}

void EditorApp::draw_shortcuts_dialog() {
  if (shortcuts_open_) {
    ImGui::OpenPopup(kShortcutsPopup);
    shortcuts_open_ = false;
  }
  if (!ImGui::BeginPopupModal(kShortcutsPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
  draw_shortcuts_table();
  if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
  ImGui::EndPopup();
}

void EditorApp::update_title() {
  const std::string title =
      editor_logic::window_title(session_->game_dir().filename().string(), session_->document().dirty(), !editing());
  if (title == title_) return;
  title_ = title;
  window_->set_title(title_);
}

void EditorApp::handle_shortcuts() {
  if (ImGui::GetIO().WantTextInput || editing_blocked()) return;
  const bool was_editing = editing();  // Play and Stop below take effect on the next frame's shortcuts
  if (ctrl_down()) handle_window_shortcuts(was_editing);
  if (was_editing) {
    handle_editing_shortcuts();
  } else if (IsKeyPressed(KEY_ESCAPE)) {
    stop();  // the only key the game does not own while playing
  }
}

// Ctrl + a key, whether a game runs or not: Play / Stop and the panel toggles (as in VS Code).
void EditorApp::handle_window_shortcuts(bool was_editing) {
  if (IsKeyPressed(KEY_P)) was_editing ? play() : stop();
  if (IsKeyPressed(KEY_B)) (alt_down() ? panels_.inspector : panels_.hierarchy) ^= true;
  if (IsKeyPressed(KEY_J)) panels_.bottom ^= true;
}

void EditorApp::handle_editing_shortcuts() {
  if (ctrl_down()) handle_document_shortcuts();
  if (IsKeyPressed(KEY_DELETE)) session_->delete_selection();
  if (ctrl_down() || IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) return;  // WASD flies the camera
  if (IsKeyPressed(KEY_F)) frame_selection();
  handle_gizmo_keys();
}

void EditorApp::handle_gizmo_keys() {
  for (const GizmoTool& tool : kGizmoTools) {
    if (IsKeyPressed(tool.key)) gizmo_ = tool.mode;
  }
}

// Ctrl + a key, while editing.
void EditorApp::handle_document_shortcuts() {
  if (IsKeyPressed(KEY_S)) save();
  if (IsKeyPressed(KEY_Z)) shift_down() ? session_->redo() : session_->undo();
  if (IsKeyPressed(KEY_Y)) session_->redo();
  if (IsKeyPressed(KEY_D)) session_->duplicate_selection();
}

void EditorApp::update_camera() {
  if (!editing() || !viewport_->hovered) return;
  FlyInput input;
  input.dt = frame_seconds();
  input.zoom = GetMouseWheelMove();
  if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
    const Vector2 delta = GetMouseDelta();
    input.look_x = delta.x;
    input.look_y = delta.y;
    steer(input, fly_keys());
  }
  camera_.update(input);
}

void EditorApp::render_viewport_texture() {
  if (viewport_->target.id == 0) return;
  Engine* runtime = session_->runtime();
  if (runtime == nullptr) {
    draw_edited_scene();
  } else if (session_->play_state() == PlayState::Paused) {
    runtime->present();  // a running game shows each of its frames itself (GameView); a paused one has none
  }
}

// The scene as it is edited, through the editor's camera, with the editor's marks over it.
void EditorApp::draw_edited_scene() {
  Viewport& view = *viewport_;
  aim_viewport_camera();
  renderer_->set_show_prefabs(true);  // templates are edited in the editor, hidden in the game
  renderer_->set_motion_blur(kNoMotionBlur);
  sync_renderer_assets();
  renderer_->draw_world(session_->scene(), view.camera, &view.target);
  BeginTextureMode(view.target);
  BeginMode3D(view.camera);
  draw_overlays();
  EndMode3D();
  EndTextureMode();
}

void EditorApp::aim_viewport_camera() {
  Camera3D& camera = viewport_->camera;
  camera.position = rl(camera_.position());
  camera.target = rl(camera_.target());
  camera.up = kWorldUp;
  camera.fovy = camera_.fov_y_degrees();
  camera.projection = CAMERA_PERSPECTIVE;
}

// Objects and materials created in the editor, and terrain that was sculpted.
void EditorApp::sync_renderer_assets() {
  renderer_->sync(session_->scene_desc());
  for (const int mesh : session_->document().take_changed_meshes()) {
    renderer_->refresh_mesh(session_->scene_desc(), mesh);
  }
}

void EditorApp::draw_overlays() {
  DrawGrid(kGridSlices, kGridSpacing);
  draw_object_marks();
  draw_selection_box();
  draw_brush();
}

void EditorApp::draw_selection_box() {
  const Scene& scene = session_->scene();
  const ObjectId selected = session_->selection();
  if (!scene.is_alive(selected)) return;
  const Bounds bounds = bounds_of(scene.at(selected), session_->scene_desc());
  draw_box(scene.world_transform(selected), bounds, rgba(kSelectionColor));
}

// The terrain brush: a ring lying on the ground, and its center.
void EditorApp::draw_brush() {
  if (!brush_point_) return;
  const Vec3 center = *brush_point_;
  DrawCircle3D(rl(center + Vec3::up() * kBrushLift), terrain_tool_.radius, kWorldRight, kQuarterTurn,
               rgba(kSelectionColor));
  DrawLine3D(rl(center), rl(center + Vec3::up() * terrain_tool_.radius * kBrushPoleShare), rgba(kSelectionColor));
}

// What a picture of the scene would not show: triggers, cameras, lights and empty objects.
void EditorApp::draw_object_marks() {
  const Scene& scene = session_->scene();
  for (const ObjectId id : scene.alive_ids()) {
    const GameObject& object = scene.at(id);
    const Transform world = scene.world_transform(id);
    if (body_config_from_props(object.props).trigger) {
      draw_box(world, bounds_of(object, session_->scene_desc()), rgba(kTriggerColor));
    }
    draw_mark(object, world);
  }
}

// --- Bars --------------------------------------------------------------------------------------

void EditorApp::draw_menu_bar() {
  if (!ImGui::BeginMainMenuBar()) return;
  ImGui::TextColored(ui::accent_color(), ICON_FA_GEM);
  ui::menu("File", [this] { draw_file_menu(); });
  ui::menu("Edit", [this] { draw_edit_menu(); });
  ui::menu("View", [this] { draw_view_menu(); });
  ui::menu("GameObject", [this] { draw_game_object_menu(); }, editing());
  ui::menu("Help", [this] { draw_help_menu(); });
  ImGui::EndMainMenuBar();
}

void EditorApp::draw_game_object_menu() {
  ui::PanelContext context = panel_context();
  ui::draw_create_menu(context);
}

void EditorApp::draw_help_menu() {
  if (ImGui::MenuItem(ICON_FA_KEYBOARD "  Keyboard Shortcuts")) shortcuts_open_ = true;
}

void EditorApp::draw_file_menu() {
  if (ImGui::MenuItem(ICON_FA_FILE_CIRCLE_PLUS "  New Game...")) request_exit(EditorExit::OpenHub);
  if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN "  Open Game...")) request_exit(EditorExit::OpenHub);
  ImGui::Separator();
  draw_game_file_items();
  ImGui::Separator();
  if (ImGui::MenuItem(ICON_FA_RIGHT_FROM_BRACKET "  Quit")) request_exit(EditorExit::Quit);
}

// What the File menu offers for the game being edited.
void EditorApp::draw_game_file_items() {
  if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK "  Save Scene", kKeysSave, false, editing())) save();
  ui::menu(ICON_FA_BOX_ARCHIVE "  Build Game", [this] { draw_build_targets(); }, editing());
  if (ImGui::MenuItem(ICON_FA_FOLDER "  Show Game Folder")) platform::open_in_system(session_->game_dir());
}

void EditorApp::draw_edit_menu() {
  draw_undo_items();
  ImGui::Separator();
  draw_selection_items();
  ImGui::Separator();
  ui::menu(ICON_FA_MICROCHIP "  Graphics Card", [this] { draw_graphics_card_menu(); });
}

void EditorApp::draw_undo_items() {
  const bool edit = editing();
  const SceneDocument& doc = session_->document();
  if (ImGui::MenuItem(ICON_FA_ROTATE_LEFT "  Undo", kKeysUndo, false, edit && doc.can_undo())) session_->undo();
  if (ImGui::MenuItem(ICON_FA_ROTATE_RIGHT "  Redo", kKeysRedo, false, edit && doc.can_redo())) session_->redo();
}

void EditorApp::draw_selection_items() {
  const bool has_selection = session_->selection() != kNoObject;
  const bool can_change = editing() && has_selection;
  if (ImGui::MenuItem(ui::kDuplicateLabel, kKeysDuplicate, false, can_change)) session_->duplicate_selection();
  if (ImGui::MenuItem(ui::kDeleteLabel, kKeysDelete, false, can_change)) session_->delete_selection();
  ImGui::Separator();
  if (ImGui::MenuItem(ICON_FA_EXPAND "  Frame Selection", kKeysFrame, false, has_selection)) frame_selection();
}

void EditorApp::draw_view_menu() {
  ImGui::MenuItem(ICON_FA_LIST "  Hierarchy (left)", kKeysHierarchy, &panels_.hierarchy);
  ImGui::MenuItem(ICON_FA_SLIDERS "  Inspector (right)", kKeysInspector, &panels_.inspector);
  ImGui::MenuItem(ICON_FA_TERMINAL "  Project and Console (bottom)", kKeysBottomPanel, &panels_.bottom);
  ImGui::Separator();
  if (ImGui::MenuItem(ICON_FA_CODE "  Focus on code and scene")) panels_ = {false, false, false};
  if (ImGui::MenuItem(ICON_FA_TABLE_COLUMNS "  Show all panels")) panels_ = {};
}

void EditorApp::draw_graphics_card_menu() {
  UserSettings settings = read_user_settings(user_settings_file());
  for (const GpuOption& option : kGpuOptions) {
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
  if (begin_toolbar()) {
    ui::toggle_button(ICON_FA_TABLE_LIST, "Show / hide the Hierarchy (Ctrl+B)", panels_.hierarchy);
    ImGui::SameLine(0.0f, kToolbarGroupGap);
    draw_gizmo_tools();
    draw_play_controls();
    draw_game_tools();
  }
  ImGui::End();
}

// Left of the toolbar: what the gizmo does and how.
void EditorApp::draw_gizmo_tools() {
  ImGui::BeginDisabled(!editing());
  for (const GizmoTool& tool : kGizmoTools) {
    if (&tool != std::begin(kGizmoTools)) ImGui::SameLine();
    if (ui::icon_button(tool.icon, tool.tooltip, gizmo_ == tool.mode)) gizmo_ = tool.mode;
  }
  ImGui::SameLine(0.0f, kToolbarGroupGap);
  if (ui::icon_button(gizmo_local_ ? ICON_FA_CUBE : ICON_FA_GLOBE,
                      gizmo_local_ ? "Local axes (click for world)" : "World axes (click for local)")) {
    gizmo_local_ = !gizmo_local_;
  }
  ImGui::SameLine();
  ui::toggle_button(ICON_FA_MAGNET, "Snap to grid (or hold Ctrl while dragging)", snapping_);
  ImGui::EndDisabled();
}

// Center of the toolbar: Play / Stop and Pause.
void EditorApp::draw_play_controls() {
  const bool edit = editing();
  const float group = icon_button_width() * kPlayControls + ImGui::GetStyle().ItemSpacing.x;
  ImGui::SameLine(editor_logic::centered_start(ImGui::GetWindowContentRegionMax().x, group));
  if (edit) {
    if (ui::icon_button(ICON_FA_PLAY, "Play (Ctrl+P)")) play();
  } else {
    ImGui::PushStyleColor(ImGuiCol_Text, ui::accent_color());
    if (ui::icon_button(ICON_FA_STOP, "Stop (Ctrl+P / Esc)", true)) stop();
    ImGui::PopStyleColor();
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(edit);
  if (ui::icon_button(ICON_FA_PAUSE, "Pause", session_->play_state() == PlayState::Paused)) toggle_pause();
  ImGui::EndDisabled();
}

// Right of the toolbar: the game's name, Build and the Inspector toggle.
void EditorApp::draw_game_tools() {
  draw_game_name();
  ImGui::SameLine();
  draw_build_button();
  ImGui::SameLine(0.0f, kToolbarGroupGap);
  ui::toggle_button(ICON_FA_TABLE_LIST, "Show / hide the Inspector (Ctrl+Alt+B)", panels_.inspector);
}

// The game's name (with a dot while the scene has unsaved changes), placed so that the buttons
// after it end at the right edge.
void EditorApp::draw_game_name() {
  const std::string game = session_->game_dir().filename().string();
  const std::string label =
      std::string(ICON_FA_GAMEPAD "  ") + game + (session_->document().dirty() ? "  " ICON_FA_CIRCLE : "");
  const float width = ImGui::CalcTextSize(label.c_str()).x;
  const float buttons = (icon_button_width() + ImGui::GetStyle().ItemSpacing.x) * kRightButtons;
  ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - width - buttons - kToolbarGroupGap);
  ImGui::AlignTextToFramePadding();
  ImGui::TextColored(ui::muted_color(), "%s", label.c_str());
}

void EditorApp::draw_build_button() {
  ImGui::BeginDisabled(!editing());
  if (ui::icon_button(ICON_FA_BOX_ARCHIVE,
                      "Build the game: an executable for Linux or Windows, or an app for Android")) {
    ImGui::OpenPopup(kBuildPopup);
  }
  ui::popup(kBuildPopup, [this] { draw_build_targets(); });
  ImGui::EndDisabled();
}

void EditorApp::draw_status_bar() {
  if (begin_status_bar()) {
    ImGui::SetCursorPosY(editor_logic::centered_start(kStatusBarHeight, ImGui::GetTextLineHeight()));
    draw_last_message();
    draw_scene_summary();
  }
  ImGui::End();
}

// Left of the status bar: the console's newest line.
void EditorApp::draw_last_message() {
  if (console_->entries().empty()) return;
  const ConsoleEntry& last = console_->entries().back();
  ImGui::TextColored(level_color(last.level), "%s", last.message.c_str());
}

// Right of the status bar: the selection, how big the scene is, the play state and the frame rate.
void EditorApp::draw_scene_summary() {
  const ObjectId selected = session_->selection();
  char info[kTextBufferSize];
  ImFormatString(info, sizeof(info), "%s    %s %d objects    %s    %d FPS",
                 selected != kNoObject ? session_->document().node(selected).name.c_str() : "", ICON_FA_CUBES,
                 session_->document().node_count(), play_state_label(session_->play_state()), GetFPS());
  ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize(info).x);
  ImGui::TextColored(ui::muted_color(), "%s", info);
}

void EditorApp::draw_dockspace() {
  const ImGuiViewport* main = ImGui::GetMainViewport();
  const ImGuiID dock = ImGui::GetID(kDockspaceId);
  if (!layout_built_) {
    layout_built_ = true;
    build_default_layout(dock);
  }
  ImGui::DockSpaceOverViewport(dock, main);
  apply_pending_split();
}

// Hierarchy on the left, Inspector on the right, Project and Console below the Scene.
void EditorApp::build_default_layout(unsigned int dock) {
  start_layout(dock);
  ImGuiID center = dock;
  const ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, kHierarchyWidth, nullptr, &center);
  const ImGuiID right =
      ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, editor_logic::inspector_split_ratio(), nullptr, &center);
  const ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, kBottomHeight, nullptr, &center);
  ImGui::DockBuilderDockWindow(kHierarchyWindow, left);
  ImGui::DockBuilderDockWindow(kInspectorWindow, right);
  ImGui::DockBuilderDockWindow(kProjectWindow, bottom);
  ImGui::DockBuilderDockWindow(kConsoleWindow, bottom);
  ImGui::DockBuilderDockWindow(kViewportWindow, center);
  ImGui::DockBuilderFinish(dock);
  center_dock_ = center;
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
  script_dock_ = split_to_the_right(*window->DockNode, name);
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

void EditorApp::draw_viewport_drop_target() {
  if (!ImGui::BeginDragDropTarget()) return;
  const Vector2 mouse = viewport_->mouse();
  if (const char* model = ui::dropped_text(kPayloadModel)) drop_model(model, mouse.x, mouse.y);
  if (const char* script = ui::dropped_text(kPayloadScript)) drop_script(script, mouse.x, mouse.y);
  ImGui::EndDragDropTarget();
}

// A model lands on the ground under the mouse (y = 0), or in front of the camera when aiming at the sky.
void EditorApp::drop_model(const char* model_file, float x, float y) {
  const Ray ray = viewport_->ray_at(x, y);
  const Vec3 at = editor_logic::drop_point(vec(ray.position), vec(ray.direction), spawn_point());
  ui::import_model(*session_, model_file, at);
}

// A script goes to the object under the mouse.
void EditorApp::drop_script(const char* script_file, float x, float y) {
  const ObjectId target = pick_at(x, y);
  if (target == kNoObject) {
    log::warning("drop scripts on an object");
    return;
  }
  session_->attach_script(target, script_file);
}

void EditorApp::draw_viewport_window() {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  const bool playing = !editing();
  if (frame_count_ <= kStartupFrames) ImGui::SetNextWindowFocus();  // the Scene tab starts in front
  ImGui::Begin(kViewportWindow, nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  draw_viewport_image();
  if (!playing) draw_viewport_drop_target();
  track_viewport_focus();
  draw_viewport_hint(playing);
  brush_point_.reset();
  playing ? draw_play_border() : use_viewport_tools();
  ImGui::End();
  ImGui::PopStyleVar();
}

// Whether the mouse and the keyboard are in the Scene view. Flying (right mouse) takes the keyboard.
void EditorApp::track_viewport_focus() {
  Viewport& view = *viewport_;
  view.hovered = ImGui::IsWindowHovered();
  view.focused = ImGui::IsWindowFocused();
  if (view.hovered && IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) ImGui::SetWindowFocus();
}

// A frame in the accent color around the running game.
void EditorApp::draw_play_border() {
  const ImVec2 origin(viewport_->screen_x, viewport_->screen_y);
  const ImVec2 end(origin.x + viewport_width(), origin.y + viewport_height());
  ImGui::GetWindowDrawList()->AddRect(origin, end, ImGui::GetColorU32(ui::accent_color()), 0.0f, 0,
                                      kPlayBorderThickness);
}

// The rendered scene, filling the window. Remembers where it is on screen.
void EditorApp::draw_viewport_image() {
  const ImVec2 size = ImGui::GetContentRegionAvail();
  Viewport& view = *viewport_;
  view.resize(static_cast<int>(size.x), static_cast<int>(size.y));
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  view.screen_x = origin.x;
  view.screen_y = origin.y;
  ImGui::Image(ImTextureRef(static_cast<ImTextureID>(view.target.texture.id)),
               ImVec2(viewport_width(), viewport_height()), kFlippedUvTopLeft, kFlippedUvBottomRight);
}

// A hint in the bottom-left corner, on a dark pill so it reads over any scene (and stays clear
// of the game's own HUD at the top).
void EditorApp::draw_viewport_hint(bool playing) {
  const Viewport& view = *viewport_;
  const bool terrain_selected = session_->is_terrain(session_->selection());
  const char* hint = kViewportHints[static_cast<int>(
      editor_logic::viewport_hint({playing, view.focused, terrain_selected, terrain_tool_.sculpting}))];
  const ImVec2 hint_size = ImGui::CalcTextSize(hint);
  const editor_logic::Rect pill =
      editor_logic::hint_pill(view.screen_x, view.screen_y, viewport_height(), hint_size.x, hint_size.y);
  ImDrawList* draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(ImVec2(pill.min_x, pill.min_y), ImVec2(pill.max_x, pill.max_y),
                      ImGui::GetColorU32(ImGuiCol_WindowBg, kOverlayAlpha), kOverlayRounding);
  draw->AddText(ImVec2(pill.min_x + kOverlayPadding, pill.min_y + kOverlayPadding),
                ImGui::GetColorU32(playing ? ui::accent_color() : ui::muted_color()), hint);
}

// What the mouse does in the Scene view while editing: the terrain brush, or the gizmo and picking.
void EditorApp::use_viewport_tools() {
  const Viewport& view = *viewport_;
  // Sculpting takes the left button over from the gizmo and from picking.
  const bool sculpting = terrain_tool_.sculpting && session_->is_terrain(session_->selection());
  if (sculpting) {
    use_brush();
    return;
  }
  stroke_started_ = false;
  if (session_->selection() != kNoObject) draw_gizmo(view.screen_x, view.screen_y, viewport_width(), viewport_height());
  if (view.hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing()) {
    const Vector2 mouse = view.mouse();
    pick_at(mouse.x, mouse.y);
  }
}

void EditorApp::use_brush() {
  const Viewport& view = *viewport_;
  const Vector2 mouse = view.mouse();
  const bool brushing = view.hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left);
  if (view.hovered) brush_point_ = sculpt_at(mouse.x, mouse.y, brushing ? frame_seconds() : 0.0f);
  if (!brushing && stroke_started_) {
    stroke_started_ = false;
    session_->document().drop_checkpoint_if_unchanged();
  }
}

void EditorApp::draw_gizmo(float x, float y, float width, float height) {
  const ObjectId selected = session_->selection();
  const Scene& scene = session_->scene();
  if (!scene.is_alive(selected) || width <= 0.0f || height <= 0.0f) return;
  ImGuizmo::SetOrthographic(false);
  ImGuizmo::SetDrawlist();
  ImGuizmo::SetRect(x, y, width, height);
  float16 model = MatrixToFloatV(model_matrix(scene.world_transform(selected)));
  manipulate(model.v, width / height);
  gizmo_drawn_ = true;
  track_gizmo_drag(ImGuizmo::IsUsing());
  if (gizmo_dragging_) apply_gizmo_drag(transform_from_matrix(model.v));
}

// Draws the gizmo at `model` (the 16 numbers of a world matrix) and lets the mouse move it.
void EditorApp::manipulate(float* model, float aspect) {
  const float16 view = MatrixToFloatV(GetCameraMatrix(viewport_->camera));
  // Same projection as raylib's BeginMode3D.
  const float16 projection = MatrixToFloatV(
      MatrixPerspective(viewport_->camera.fovy * DEG2RAD, aspect, rlGetCullDistanceNear(), rlGetCullDistanceFar()));
  // Scaling always works on the object's own axes.
  const ImGuizmo::MODE mode = gizmo_local_ || gizmo_ == GizmoMode::Scale ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
  const bool snap = snapping_ || ImGui::GetIO().KeyCtrl;
  const float step = snap_step(gizmo_);
  const float snap_values[] = {step, step, step};
  ImGuizmo::Manipulate(view.v, projection.v, gizmo_operation(gizmo_), mode, model, nullptr,
                       snap ? snap_values : nullptr);
}

// One undo step per drag, dropped when the drag ends where it started.
void EditorApp::track_gizmo_drag(bool using_gizmo) {
  if (using_gizmo && !gizmo_dragging_) session_->document().checkpoint();
  if (!using_gizmo && gizmo_dragging_) session_->document().drop_checkpoint_if_unchanged();
  gizmo_dragging_ = using_gizmo;
}

// Moves the selection to where the gizmo was dragged (a world placement).
void EditorApp::apply_gizmo_drag(const Transform& world) {
  const ObjectId selected = session_->selection();
  const NodeDesc& node = session_->document().node(selected);
  const Transform local =
      node.parent == kNoIndex ? world : relative_to(session_->scene().world_transform(node.parent), world);
  session_->document().set_local(selected, local, false);
  session_->refresh_preview();
}

}  // namespace redgem
