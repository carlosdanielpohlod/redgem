#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "audio/audio.hpp"
#include "editor/console_log.hpp"
#include "editor/editor_camera.hpp"
#include "editor/editor_session.hpp"
#include "editor/project_files.hpp"
#include "editor/script_workspace.hpp"
#include "editor/ui/panels.hpp"
#include "input/raylib_input_source.hpp"
#include "render/gpu_select.hpp"

namespace redgem {

class Renderer;

enum class GizmoMode { Translate, Rotate, Scale };

// Why the editor window closed: quit, or go back to the project hub (New / Open game).
enum class EditorExit { Quit, OpenHub };

// How long the frame about to be drawn counts for, in seconds; asked once a frame.
using FrameClock = std::function<float()>;
// The time the window measured between its last two frames.
FrameClock window_frame_clock();

// One way of making a standalone game, as Build Game offers it. The editor only knows that such
// ways exist: whoever opens it says which, and does the building.
struct BuildChoice {
  std::string name;   // the system built for ("linux"): how the build is spoken of, and its folder named
  std::string label;  // what the menu shows
  // Whether it is for the system the editor runs on (the build then needs no system in its folder's name).
  bool for_this_system = false;
  // Builds the game in `game_dir` into `out_dir`. The game's executable (or app), or nullopt
  // with `error` saying why not.
  std::function<std::optional<std::filesystem::path>(const std::filesystem::path& game_dir,
                                                     const std::filesystem::path& out_dir, std::string& error)>
      build;
};

// What whoever opens the editor decides for it.
struct EditorOptions {
  // How long each frame counts for: the window's own time unless whoever drives the editor steps
  // it at a pace of its own.
  FrameClock clock = window_frame_clock();
  // The graphics card the window opens on, where the machine has two.
  GpuChoice gpu = GpuChoice::Auto;
  // The ways of building a game, in the order Build Game lists them. None: the editor builds nothing.
  std::vector<BuildChoice> builds;
};

// Keyboard and mouse reach the game only while the Scene view has focus during play. The game
// sees the mouse in the view's own pixels, as if the view were its window.
class GatedInputSource final : public InputSource {
 public:
  bool is_key_down(Key key) const override { return enabled && device.is_key_down(key); }
  MouseSample mouse() const override {
    MouseSample sample = device.mouse();
    sample.x -= origin_x;
    sample.y -= origin_y;
    if (!enabled) sample.buttons = {}, sample.wheel = 0.0f;
    return sample;
  }
  GamepadSample gamepad() const override { return enabled ? device.gamepad() : GamepadSample{}; }
  std::string typed_text() const override { return enabled ? device.typed_text() : std::string(); }
  bool enabled = false;
  float origin_x = 0.0f;  // where the Scene view's image starts in the window
  float origin_y = 0.0f;
  RaylibInputSource device;
};

// The editor window: toolbar, Hierarchy, Scene viewport (gizmos, Play mode), Inspector, Project,
// Console and script tabs, status bar.
class EditorApp {
 public:
  static std::variant<std::unique_ptr<EditorApp>, std::string> create(const std::filesystem::path& game_dir,
                                                                      EditorOptions options = {});
  ~EditorApp();
  EditorApp(const EditorApp&) = delete;
  EditorApp& operator=(const EditorApp&) = delete;

  // Runs until the window is closed: frame() after frame() until finished(). Returns the process
  // exit code.
  int run();
  // One editor frame (input, simulation while playing, UI, rendering). What run() is a loop of,
  // for whoever drives the editor a frame at a time.
  void frame();
  // Whether the editor is done: leaving was asked for and nothing unsaved stands in the way (there
  // was nothing, or the user chose Save / Discard).
  bool finished() const { return quit_confirmed_; }

  EditorSession& session() { return *session_; }
  EditorCamera& camera() { return camera_; }
  const ConsoleLog& console() const { return *console_; }
  ScriptWorkspace& scripts() { return *scripts_; }
  // Side and bottom panels can be hidden to give the scene or the code more room.
  struct PanelVisibility {
    bool hierarchy = true;  // left (Ctrl+B)
    bool inspector = true;  // right (Ctrl+Alt+B)
    bool bottom = true;     // Project and Console (Ctrl+J)
  };
  PanelVisibility& panels() { return panels_; }
  // The terrain brush (what the Inspector's Terrain card edits).
  ui::TerrainTool& terrain_tool() { return terrain_tool_; }
  // One step of a brush stroke on the selected terrain where a ray of the Scene view meets it
  // (viewport pixels, like pick_at); `seconds` is how long the brush was held there. Returns
  // where it touched the ground.
  std::optional<Vec3> sculpt_at(float x, float y, float seconds);
  // Moves a script tab into a new split on the right of its group (next frame).
  void split_script_right(const std::string& file_name);
  GizmoMode gizmo_mode() const { return gizmo_; }
  const ProjectFiles& files() const { return files_; }
  bool close_requested() const;
  EditorExit exit_action() const { return exit_action_; }

  // Commands, the same ones the menus and shortcuts use.
  void play();
  void stop();
  void toggle_pause();
  void save();
  void frame_selection();
  // Asks to leave (after the unsaved-changes dialog when needed).
  void request_exit(EditorExit action);
  // Makes a standalone game in a folder next to the game ("<game>-build", or "<game>-build-windows"
  // for another system), in one of the ways the editor was opened with. Returns that folder.
  std::optional<std::filesystem::path> build(const BuildChoice& choice);
  // The same, for the system the editor runs on.
  std::optional<std::filesystem::path> build();
  const std::vector<BuildChoice>& builds() const { return builds_; }
  // Where objects created from the menus appear: in front of the editor camera.
  Vec3 spawn_point() const;
  // Re-reads the game folder (scripts, models, sounds, scenes).
  void rescan_files();
  // Selects what is under a point of the viewport (in viewport pixels, origin top-left).
  ObjectId pick_at(float x, float y);
  float viewport_width() const;
  float viewport_height() const;

 private:
  EditorApp() = default;
  void open_window(GpuChoice gpu);
  bool editing() const;  // not playing
  ui::PanelContext panel_context();
  // Before drawing: files changed outside, input, the running game.
  void update_before_drawing();
  void check_files_on_disk();
  void handle_shortcuts();
  void handle_window_shortcuts(bool was_editing);
  void handle_editing_shortcuts();
  void handle_document_shortcuts();
  void handle_gizmo_keys();
  void update_camera();
  void feed_game();
  // The Scene view's picture.
  void render_viewport_texture();
  void aim_viewport_camera(const Scene& scene);
  void sync_renderer_assets();
  void draw_over_world(Engine* playing);
  void draw_overlays();
  void draw_object_marks();
  void draw_selection_box();
  void draw_brush();
  // The interface, top to bottom.
  void draw_interface();
  void draw_menu_bar();
  void draw_file_menu();
  void draw_game_file_items();
  void draw_edit_menu();
  void draw_undo_items();
  void draw_selection_items();
  void draw_view_menu();
  void draw_game_object_menu();
  void draw_help_menu();
  void draw_graphics_card_menu();
  void draw_build_targets();
  void draw_toolbar();
  void draw_gizmo_tools();
  void draw_play_controls();
  void draw_game_tools();
  void draw_game_name();
  void draw_build_button();
  void draw_status_bar();
  void draw_last_message();
  void draw_scene_summary();
  void draw_dockspace();
  void build_default_layout(unsigned int dock);
  void apply_pending_split();
  unsigned int new_script_dock() const;
  void draw_panels();
  void draw_side_panels(ui::PanelContext& context);
  void draw_script_tabs();
  void draw_viewport_window();
  void draw_viewport_image();
  void draw_viewport_drop_target();
  // What was dropped on a point of the Scene view (viewport pixels).
  void drop_model(const char* model_file, float x, float y);
  void drop_script(const char* script_file, float x, float y);
  void track_viewport_focus();
  void draw_play_border();
  void draw_viewport_hint(bool playing);
  void use_viewport_tools();
  void use_brush();
  void draw_gizmo(float x, float y, float width, float height);
  void manipulate(float* model, float aspect);
  void track_gizmo_drag(bool using_gizmo);
  void end_abandoned_gizmo_drag();
  void apply_gizmo_drag(const Transform& world);
  void update_title();
  void draw_dialogs();
  void draw_unsaved_dialog();
  void draw_unsaved_choices();
  void save_and_leave();
  void leave_without_saving();
  void cancel_leaving();
  void draw_external_change_dialog();
  void draw_external_change_choices();
  void draw_shortcuts_dialog();
  bool editing_blocked() const;
  void save_scripts();
  float frame_seconds() const { return frame_seconds_; }

  std::unique_ptr<ConsoleLog> console_;
  std::unique_ptr<Renderer> renderer_;
  std::unique_ptr<AudioSystem> audio_;
  int runtime_scene_generation_ = 0;  // which runtime scene the renderer's assets belong to
  std::unique_ptr<EditorSession> session_;
  std::unique_ptr<ScriptWorkspace> scripts_;
  PanelVisibility panels_;
  ui::TerrainTool terrain_tool_;
  bool stroke_started_ = false;               // a brush stroke is in progress (one undo step)
  std::optional<Vec3> brush_point_;           // where the brush is on the ground this frame, if it is over it
  std::optional<std::string> pending_split_;  // window name of a script tab to split off
  unsigned int center_dock_ = 0;              // where the Scene and new script tabs start
  unsigned int script_dock_ = 0;              // dock of the last focused script tab
  EditorCamera camera_;
  GatedInputSource game_input_;
  GizmoMode gizmo_ = GizmoMode::Translate;
  ProjectFiles files_;
  bool snapping_ = false;
  bool gizmo_local_ = false;  // gizmo axes of the object instead of the world
  EditorExit exit_action_ = EditorExit::Quit;
  bool shortcuts_open_ = false;
  FrameClock clock_;
  std::vector<BuildChoice> builds_;
  float frame_seconds_ = 0.0f;  // what the clock said of the frame being drawn
  struct Viewport;
  std::unique_ptr<Viewport> viewport_;
  long long frame_count_ = 0;
  bool layout_built_ = false;
  bool gizmo_dragging_ = false;
  bool close_pending_ = false;   // leaving was asked for (close button, Quit, New / Open Game) with unsaved work
  bool quit_confirmed_ = false;  // the user chose Save or Discard
  bool gizmo_drawn_ = false;
  std::string title_;
};

}  // namespace redgem
