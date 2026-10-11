#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "app/game_project.hpp"
#include "audio/audio.hpp"
#include "editor/console_log.hpp"
#include "editor/editor_camera.hpp"
#include "editor/editor_session.hpp"
#include "editor/project_files.hpp"
#include "editor/script_workspace.hpp"
#include "editor/ui/panels.hpp"
#include "input/raylib_input_source.hpp"

namespace redgem {

class Renderer;

enum class GizmoMode { Translate, Rotate, Scale };

// Why the editor window closed: quit, or go back to the project hub (New / Open game).
enum class EditorExit { Quit, OpenHub };

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
  bool enabled = false;
  float origin_x = 0.0f;  // where the Scene view's image starts in the window
  float origin_y = 0.0f;
  RaylibInputSource device;
};

// The editor window: toolbar, Hierarchy, Scene viewport (gizmos, Play mode), Inspector, Project,
// Console and script tabs, status bar.
class EditorApp {
 public:
  static std::variant<std::unique_ptr<EditorApp>, std::string> create(const std::filesystem::path& game_dir);
  ~EditorApp();
  EditorApp(const EditorApp&) = delete;
  EditorApp& operator=(const EditorApp&) = delete;

  // Runs until the window is closed. Returns the process exit code.
  int run();
  // One editor frame (input, simulation while playing, UI, rendering).
  void frame();

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
  // True once the window is about to close (nothing unsaved, or the user chose Save / Discard).
  bool exit_confirmed() const { return quit_confirmed_; }

  // Commands, the same ones the menus and shortcuts use.
  void play();
  void stop();
  void toggle_pause();
  void save();
  void frame_selection();
  // Asks to leave (after the unsaved-changes dialog when needed).
  void request_exit(EditorExit action);
  // Makes a standalone game in "<game>-build": an executable and the game's files. Returns that folder.
  // Makes a standalone game for a system in a folder next to the game ("<game>-build", or
  // "<game>-build-windows"). Returns that folder.
  std::optional<std::filesystem::path> build(BuildTarget target = host_build_target());
  // Where objects created from the menus appear: in front of the editor camera.
  Vec3 spawn_point() const;
  // Re-reads the game folder (scripts, models, sounds, scenes).
  void rescan_files();
  // Selects what is under a point of the viewport (in viewport pixels, origin top-left).
  ObjectId pick_at(float x, float y);
  float viewport_width() const;
  float viewport_height() const;
  // Makes every frame count as this long, whatever it really took (tests and recordings step the
  // editor at an even pace). nullopt goes back to real time.
  void set_frame_seconds(std::optional<float> seconds) { fixed_frame_seconds_ = seconds; }
  // Writes the whole editor window to a PNG at the end of the next frame.
  void request_screenshot(const std::filesystem::path& path) { screenshot_ = path; }

 private:
  EditorApp() = default;
  void handle_shortcuts();
  void update_camera();
  void render_viewport_texture();
  void draw_overlays();
  void draw_dockspace();
  void draw_menu_bar();
  void draw_toolbar();
  void draw_graphics_card_menu();
  void draw_build_targets();
  void draw_status_bar();
  void draw_viewport_drop_target(float x, float y);
  void apply_pending_split();
  unsigned int new_script_dock() const;
  void draw_viewport_window();
  void draw_gizmo(float x, float y, float width, float height);
  void update_title();
  void draw_dialogs();
  bool editing_blocked() const;
  float frame_seconds() const;

  std::unique_ptr<ConsoleLog> console_;
  std::unique_ptr<Renderer> renderer_;
  std::unique_ptr<AudioSystem> audio_;
  int runtime_scene_generation_ = 0;  // which runtime scene the renderer's assets belong to
  std::unique_ptr<EditorSession> session_;
  std::unique_ptr<ScriptWorkspace> scripts_;
  PanelVisibility panels_;
  ui::TerrainTool terrain_tool_;
  bool stroke_started_ = false;       // a brush stroke is in progress (one undo step)
  std::optional<Vec3> brush_point_;   // where the brush is on the ground this frame, if it is over it
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
  std::optional<std::filesystem::path> screenshot_;
  std::optional<float> fixed_frame_seconds_;
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
