#pragma once

// The editor window: toolbar, Hierarchy, Scene viewport (gizmos, Play mode), Inspector, Project,
// Console and script tabs, status bar. One class, implemented part by part (editor_frame,
// editor_commands, editor_builds, editor_input, editor_menus, editor_bars, editor_layout,
// editor_dialogs, editor_viewport, editor_scene_view, editor_scenes). This header stays long because it lists every
// step of those parts: they all work on the one window's state, which is declared once, here.

#include <filesystem>
#include <memory>
#include <optional>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "audio/audio.hpp"
#include "editor/box_select.hpp"
#include "editor/build_job.hpp"
#include "editor/console_log.hpp"
#include "editor/editor_camera.hpp"
#include "editor/editor_options.hpp"
#include "editor/editor_session.hpp"
#include "editor/file_drop.hpp"
#include "editor/gated_input_source.hpp"
#include "editor/gizmo.hpp"
#include "editor/group_transform.hpp"
#include "editor/project_files.hpp"
#include "editor/scripts/script_workspace.hpp"
#include "editor/ui/agent_panel.hpp"
#include "editor/ui/panels.hpp"

namespace redgem {

class GameView;
class Renderer;
struct SceneViewport;
class Window;
namespace ui {
class Interface;
}

// What the scene name dialog asks a name for: a new scene, or where to save the open one.
enum class SceneNameDialog { None, NewScene, SaveAs };

// What waits for the user to say what becomes of the unsaved edits (Save, Discard or Cancel):
// leaving the editor, opening another scene, or making a new one and opening it.
struct PendingAction {
  enum class Kind { Leave, OpenScene, NewScene };
  Kind kind = Kind::Leave;
  std::string scene;  // the scene to open or make
};

// Why the editor window closed: quit, or go back to the project hub (New / Open game).
enum class EditorExit { Quit, OpenHub };

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
  // Local tooling is polled on the editor thread before input and drawing. Empty by default.
  void set_frame_hook(std::function<void(EditorApp&)> hook) { frame_hook_ = std::move(hook); }
  bool interaction_active() const;
  struct ViewportPicture {
    std::vector<std::uint8_t> bytes;
    int width = 0;
    int height = 0;
  };
  // A bounded JPEG of the last completed scene viewport (not the desktop or other windows).
  std::optional<ViewportPicture> capture_viewport(int max_side, std::string& error) const;
  // Whether the editor is done: leaving was asked for, and nothing unsaved stands in the way (there
  // was nothing, or the user chose Save / Discard). Leaving waits for a build that runs, saying
  // so, and only then asks about unsaved work; no build is ever left running.
  bool finished() const { return quit_confirmed_ && !building(); }

  EditorSession& session() { return *session_; }
  EditorCamera& camera() { return camera_; }
  const ConsoleLog& console() const { return *console_; }
  ScriptWorkspace& scripts() { return *scripts_; }
  // Side and bottom panels can be hidden to give the scene or the code more room.
  struct PanelVisibility {
    bool hierarchy = true;  // left (Ctrl+B)
    bool inspector = true;  // right (Ctrl+Alt+B)
    bool bottom = true;     // Project and Console (Ctrl+J)
    bool agent = true;      // the Agent panel (right, beside the Inspector)
  };
  PanelVisibility& panels() { return panels_; }
  // What the panels remember between frames (searches, half-typed names, the console's filters):
  // this editor's own, new with every game opened.
  ui::PanelState& panel_state() { return panel_state_; }
  // The terrain brush (what the Inspector's Terrain card edits).
  ui::TerrainTool& terrain_tool() { return terrain_tool_; }
  // One step of a brush stroke on the selected terrain where a ray of the Scene view meets it
  // (viewport pixels, like pick_at); `seconds` is how long the brush was held there. Returns
  // where it touched the ground.
  std::optional<Vec3> sculpt_at(float x, float y, float seconds);
  // Moves a script tab into a new split on the right of its group (next frame).
  void split_script_right(const std::string& file_name);
  GizmoMode gizmo_mode() const { return gizmo_.mode(); }
  const ProjectFiles& files() const { return files_; }
  // Files from anywhere on the desk become the game's models and sounds (copied into its folders);
  // the Project panel shows them, and the Console says what became of each.
  void add_files(const std::vector<std::filesystem::path>& files);
  bool close_requested() const;
  EditorExit exit_action() const { return exit_action_; }
  // The conversation with a coding agent (the Agent panel).
  ui::AgentPanel& agent_panel() { return *agent_panel_; }

  // Commands, the same ones the menus and shortcuts use.
  void play();
  void stop();
  void toggle_pause();
  void save();
  // The game's scenes, by the names scripts load them with ("main" is scene.glb). Opens another
  // one, after asking to save this one's edits when it has any.
  void open_scene(const std::string& name);
  // Makes scenes/<name>.glb with what a new scene starts with and opens it, after asking like
  // open_scene (nothing is made when the user cancels). False, with why in the console, when no
  // scene can be made with that name.
  bool new_scene(const std::string& name);
  // Saves the open scene as scenes/<name>.glb, which is then the one edited.
  bool save_scene_as(const std::string& name);
  // What waits for the user's answer about the unsaved edits, if anything.
  const std::optional<PendingAction>& pending_action() const { return pending_; }
  void frame_selection();
  // Asks to leave (after the unsaved-changes dialog when needed, and once a running build ended).
  void request_exit(EditorExit action);
  // Starts making a standalone game in a folder next to the game ("<game>-build", or
  // "<game>-build-windows" for another system), in one of the ways the editor was opened with, on a
  // thread of its own: the editor keeps drawing, shows how the build goes and then how it ended.
  // The scene and the scripts are saved first, and the build works from the files on disk. False,
  // with why in the console, while another build runs or when the game cannot be saved.
  bool start_build(const BuildChoice& choice);
  // The same, waiting for the build to end (for tools and tests). Returns the folder it filled.
  std::optional<std::filesystem::path> build(const BuildChoice& choice);
  // The same, for the system the editor runs on.
  std::optional<std::filesystem::path> build();
  const std::vector<BuildChoice>& builds() const { return builds_; }
  // A build runs, or ended and was not taken in yet (it is at the next frame).
  bool building() const { return build_job_.busy(); }
  // How the last build ended, while the editor shows it (until closed, or another build starts).
  const std::optional<BuildOutcome>& last_build() const { return last_build_; }
  // Where objects created from the menus appear: in front of the editor camera.
  Vec3 spawn_point() const;
  // Re-reads the game folder (scripts, models, sounds, scenes).
  void rescan_files();
  // Selects what is under a point of the viewport (in viewport pixels, origin top-left).
  ObjectId pick_at(float x, float y);
  // The same, with what the keys held do to a click (see view_click_mode): the object alone,
  // or added to or taken from the selection.
  ObjectId pick_at(float x, float y, ClickMode mode);
  // Selects the objects whose middle shows inside a rectangle of the viewport (viewport pixels),
  // in place of the selection or added to it.
  void select_in_box(const editor_logic::Rect& rect, BoxMode mode);
  // The clipboard: the selected objects with everything under them, as text on the system's
  // clipboard (so they paste in another editor window too) or, when too big for it, in this
  // editor's. Paste puts them next to the selected object, or at the top level; paste_into under
  // `parent`.
  void copy_selection();
  void cut_selection();
  void paste();
  void paste_into(ObjectId parent);
  float viewport_width() const;
  float viewport_height() const;

 private:
  EditorApp() = default;
  void open_window(GpuChoice gpu, FrameClock clock);
  void make_agent_panel(const AgentHooks& hooks);
  // The selected objects as `describe` shows each.
  std::vector<std::string> selection_text(const std::function<std::string(const SceneDesc&, int)>& describe) const;
  // Start looking through the scene's own camera, like the game would.
  void look_through_main_camera();
  // What the Project panel asked for: a scene to open, or a new one.
  void take_scene_requests(const ui::PanelContext& context);
  // Opens a scene now, whatever the edits of the open one (see open_scene). The panels forget
  // what they held of the last one.
  void switch_scene(const std::string& name);
  bool make_and_open_scene(const std::string& name);
  bool editing() const { return session_->editing(); }
  bool unsaved_work() const;
  ui::PanelContext panel_context();
  // Before drawing: files changed outside, input, the running game.
  void update_before_drawing();
  void check_files_on_disk();
  // Every so many frames, the scene is loaded again when it changed on disk. True when it was.
  bool reloaded_from_disk();
  bool time_to(long long every_frames) const { return frame_count_ % every_frames == 0; }
  // Files dragged from the desktop onto the window (see add_files).
  void take_dropped_files();
  void report_drop(const DropReport& report);
  void handle_shortcuts();
  bool shortcuts_blocked() const;
  void handle_window_shortcuts(bool was_editing);
  void handle_editing_shortcuts();
  void handle_document_shortcuts();
  void handle_clipboard_shortcuts();
  void update_camera();
  // Editing, with the mouse over the Scene view.
  bool mouse_steers_camera() const;
  void feed_game();
  // The game reads the keys and the mouse: it runs, and the Scene view has the focus.
  bool game_has_input() const;
  // The view the game was shown in is done with: the renderer goes back to the edited scene.
  void retire_game_view();
  // The Scene view's picture.
  void render_viewport_texture();
  void draw_edited_scene();
  void aim_viewport_camera();
  void sync_renderer_assets();
  // The interface, top to bottom.
  void draw_interface();
  void draw_menu_bar();
  void draw_file_menu();
  void draw_game_file_items();
  void draw_scene_items();
  void draw_open_scene_items();
  void draw_edit_menu();
  void draw_undo_items();
  bool undo_offered() const;
  bool redo_offered() const;
  void draw_selection_items();
  void draw_clipboard_items();
  bool selection_editable() const;
  void draw_view_menu();
  void draw_game_object_menu();
  void draw_help_menu();
  void draw_graphics_card_menu();
  void draw_build_targets();
  // Where a build in `choice`'s way goes.
  std::filesystem::path build_folder(const BuildChoice& choice) const;
  // Takes in how the build ended, once it has: its messages and result go to the console, and the
  // result is shown. Returns the folder it filled, when it succeeded.
  std::optional<std::filesystem::path> take_build_outcome();
  // Builds can start: the game is being edited and no other build runs.
  bool build_offered() const { return editing() && !building(); }
  // The window in a corner of the Scene view that shows how a build goes and how it ended.
  void draw_build_status();
  void draw_build_progress();
  void draw_build_result(const BuildOutcome& outcome);
  void draw_build_result_actions(const BuildOutcome& outcome);
  // Like an IDE's "save all" before a build. False, with why in the console, when something stays
  // unsaved.
  bool saved_for_build();
  // Leaving was asked for while a build ran: it is asked again once the build ended.
  void leave_if_asked();
  // Leaving was asked for, and waits for the build to end.
  bool waiting_for_build() const { return leave_after_build_ && building(); }
  void draw_build_wait_dialog();
  bool build_wait_dialog_wanted() const;
  void draw_toolbar();
  void draw_play_controls();
  void draw_play_button(bool edit);
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
  bool script_dock_exists() const;
  void draw_panels();
  void draw_side_panels(ui::PanelContext& context);
  void draw_hierarchy_panel(ui::PanelContext& context);
  // What an object's menu in the Hierarchy asked of the clipboard.
  void run_clipboard_command(ui::ClipboardCommand command, ObjectId paste_parent);
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
  // The brush is on, and a terrain is selected.
  bool sculpting() const;
  void use_brush();
  bool stroke_ends(bool brushing) const { return !brushing && stroke_started_; }
  void draw_gizmo();
  // Something is selected, and the view has a size to draw it in.
  bool gizmo_drawable() const;
  // Where the gizmo of the selection is (see group_transform::pivot_of).
  Transform selection_pivot() const;
  std::vector<Transform> worlds_of(const std::vector<int>& objects) const;
  // Clicks and rectangles in the Scene view that select.
  void use_selection_clicks();
  void finish_selection_click(box_select::ViewPoint from, box_select::ViewPoint to);
  void draw_selection_rect(box_select::ViewPoint from, box_select::ViewPoint to) const;
  // What a rectangle can select: objects drawn with a mesh, lights and cameras, where their
  // middle shows on the view.
  std::vector<box_select::Candidate> box_candidates() const;
  box_select::ViewCamera view_camera() const;
  // The object under a point of the viewport, or kNoObject.
  ObjectId object_at(float x, float y) const;
  // Copied objects as the system's clipboard holds them, or else as this editor's does.
  std::optional<std::vector<std::uint8_t>> clipboard_objects() const;
  void keep_copied(const std::vector<std::uint8_t>& copied);
  // One undo step per drag, dropped when the drag ends where it started.
  void track_gizmo_drag(bool using_gizmo);
  bool drag_begins(bool using_gizmo) const { return using_gizmo && !gizmo_dragging_; }
  bool drag_ends(bool using_gizmo) const { return !using_gizmo && gizmo_dragging_; }
  // A drag that ended without the gizmo being drawn (the selection was cleared, play started).
  void end_abandoned_gizmo_drag();
  bool gizmo_drag_abandoned() const { return gizmo_dragging_ && !gizmo_drawn_; }
  void apply_gizmo_drag(const Transform& pivot);
  void update_title();
  void draw_dialogs();
  void draw_unsaved_dialog();
  bool unsaved_dialog_wanted() const;
  const char* unsaved_question() const;
  bool leaving_pending() const { return pending_ && pending_->kind == PendingAction::Kind::Leave; }
  void draw_unsaved_choices();
  void save_and_go_on();
  void go_on_without_saving();
  void cancel_pending();
  // Takes the pending action away (closing its dialog).
  PendingAction take_pending();
  // The edits the action would lose are still unsaved (saving them failed).
  bool still_unsaved(const PendingAction& action) const;
  void carry_out(const PendingAction& action);
  void ask_scene_name(SceneNameDialog dialog);
  void draw_scene_name_dialog();
  void draw_scene_name_form();
  bool use_scene_name(const std::string& name);
  void draw_external_change_dialog();
  bool external_change_dialog_wanted() const;
  void draw_external_change_choices();
  void draw_shortcuts_dialog();
  bool editing_blocked() const;
  void save_scripts();
  float frame_seconds() const { return frame_seconds_; }

  std::unique_ptr<ConsoleLog> console_;
  // The window, then what draws in it: made in this order, released in the opposite one.
  std::unique_ptr<Window> window_;
  std::unique_ptr<Renderer> renderer_;
  std::unique_ptr<ui::Interface> interface_;
  std::unique_ptr<AudioSystem> audio_;
  std::unique_ptr<EditorSession> session_;
  std::unique_ptr<ScriptWorkspace> scripts_;
  std::unique_ptr<ui::AgentPanel> agent_panel_;  // (its agent stops before the session goes)
  PanelVisibility panels_;
  ui::PanelState panel_state_;
  ui::TerrainTool terrain_tool_;
  bool stroke_started_ = false;               // a brush stroke is in progress (one undo step)
  std::optional<Vec3> brush_point_;           // where the brush is on the ground this frame, if it is over it
  std::optional<std::string> pending_split_;  // window name of a script tab to split off
  unsigned int center_dock_ = 0;              // where the Scene and new script tabs start
  unsigned int script_dock_ = 0;              // dock of the last focused script tab
  EditorCamera camera_;
  GatedInputSource game_input_;
  Gizmo gizmo_;
  ProjectFiles files_;
  EditorExit exit_action_ = EditorExit::Quit;
  bool shortcuts_open_ = false;
  FrameClock clock_;
  std::function<void(EditorApp&)> frame_hook_{};
  std::vector<BuildChoice> builds_;
  // The build in the background. (Its destructor waits for it: its thread only works on what the
  // job holds, and the files on disk, so it may go at any point.)
  BuildJob build_job_;
  std::optional<BuildOutcome> last_build_;  // shown until closed
  bool leave_after_build_ = false;          // leaving was asked for while a build ran
  float frame_seconds_ = 0.0f;              // what the clock said of the frame being drawn
  std::unique_ptr<SceneViewport> viewport_;
  std::unique_ptr<GameView> game_view_;  // who shows the running game, while one runs
  long long frame_count_ = 0;
  bool layout_built_ = false;
  bool gizmo_dragging_ = false;
  group_transform::Drag group_drag_;                // the gizmo drag in progress, from where it began
  std::optional<box_select::ViewPoint> box_start_;  // where the left button went down in the view, while held
  std::vector<std::uint8_t> clipboard_;             // the last objects copied in this editor
  std::optional<PendingAction> pending_;            // what waits for Save / Discard / Cancel about unsaved work
  bool quit_confirmed_ = false;                     // the user chose Save or Discard
  bool gizmo_drawn_ = false;
  std::string title_;
  SceneNameDialog scene_name_dialog_ = SceneNameDialog::None;
  bool scene_name_dialog_opening_ = false;  // the dialog opens on the next frame
  std::string scene_name_field_;
};

}  // namespace redgem
