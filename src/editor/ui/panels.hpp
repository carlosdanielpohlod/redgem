#pragma once

#include <optional>
#include <string>

#include "audio/audio.hpp"
#include "core/math.hpp"
#include "editor/console_log.hpp"
#include "editor/editor_config.hpp"
#include "editor/editor_session.hpp"
#include "editor/project_files.hpp"
#include "editor/scripts/script_workspace.hpp"
#include "editor/ui/panel_state.hpp"

#include "scene/shapes/terrain.hpp"

namespace redgem::ui {

// The terrain brush: set up in the Inspector, used in the Scene view.
struct TerrainTool {
  bool sculpting = false;  // while on, dragging the left button over the selected terrain reshapes it
  SculptMode mode = SculptMode::Raise;
  float radius = editor_config::kDefaultBrushRadius;  // meters
  // Meters per second for Raise and Lower; how fast Smooth and Flatten act.
  float strength = editor_config::kDefaultBrushStrength;
};

// What every panel works with.
struct PanelContext {
  EditorSession& session;
  ScriptWorkspace& scripts;
  const ProjectFiles& files;
  PanelState& state;             // what the panels remember between frames: the editor's own
  Vec3 spawn_point;              // where new objects appear (in front of the editor camera)
  AudioSystem* audio = nullptr;  // sound previews in the Project panel
  bool files_changed = false;    // set by panels that create files: the editor rescans the game
  // Set by the Project panel: a scene to open (double clicked), and New scene asked for.
  std::optional<std::string> scene_to_open{};
  bool new_scene_asked = false;
  TerrainTool* terrain = nullptr;
};

// What an object's menu in the Hierarchy asks of the clipboard, which the editor holds.
enum class ClipboardCommand { None, Cut, Copy, Paste, PasteInto };

// Tree of scene objects with search. Click selects (Ctrl adds or takes away, Shift selects a run
// of rows), double click asks to frame it, drag to reparent, right click for actions; scripts and
// models can be dropped on it.
struct HierarchyResult {
  bool frame_requested = false;
  ClipboardCommand clipboard = ClipboardCommand::None;
  ObjectId paste_parent = kNoObject;  // under which object PasteInto pastes (kNoObject = top level)
};
HierarchyResult draw_hierarchy(PanelContext& context);

// The selection as component cards (Transform, Mesh & material, Light, Camera, one card per
// script, Physics, Custom properties) and an Add Component button.
void draw_inspector(PanelContext& context);

// The game's files: scripts (double click edits, drag onto objects), models (double click or
// drag to add to the scene), sounds (double click plays) and scenes (double click opens, New
// scene makes one).
void draw_project(PanelContext& context);

void draw_console(ConsoleLog& console, ConsoleState& state);

// One code editor tab per open script (dockable: drag tabs to split, like a code editor). Ctrl+S
// saves with a syntax check. New tabs dock into `new_tab_dock`.
struct ScriptTabsResult {
  std::optional<std::string> split_right;  // window name of a tab to move into a split on its right
  unsigned int focused_dock = 0;           // dock node of the focused script tab, if any
};
ScriptTabsResult draw_script_tabs(ScriptWorkspace& scripts, ScriptTabsState& state, unsigned int new_tab_dock);
// Window name of a script's tab.
std::string script_window_name(const std::string& file_name);

// Menu items that create objects (GameObject menu, hierarchy "+" and context menus).
void draw_create_menu(PanelContext& context);

// GameObject > Terrain: new ground where new objects appear, selected, with the brush already on.
ObjectId create_terrain(PanelContext& context);

// Adds a model of the game's models/ folder to the scene at a world position, optionally under
// another object. Says why in the console when it cannot. Returns the new object or kNoObject.
ObjectId import_model(EditorSession& session, const std::string& model_file, Vec3 position,
                      ObjectId parent = kNoObject);

// What a name field takes: who says whether a name can be used, and the key of the text of the
// rule a name that cannot breaks.
struct NameRule {
  bool (*usable)(const std::string& name);
  const char* rule;
};
// A field for a new file's name with a button next to it, and under them the rule a name that
// cannot be used breaks. True when the button (or Enter) asked for a usable name.
bool draw_name_form(const char* id, const char* hint, const char* button_label, const NameRule& rule,
                    std::string& name);
// The same, for a new script's name (scripts/<name>.rb) ...
bool draw_new_script_form(const char* id, const char* hint, const char* button_label, std::string& name);
// ... and for a new scene's (scenes/<name>.glb).
bool draw_new_scene_form(const char* id, const char* hint, const char* button_label, std::string& name);

// Opens a script in its own tab (or brings the tab to the front).
void open_script(ScriptWorkspace& scripts, const std::string& file_name);

}  // namespace redgem::ui
