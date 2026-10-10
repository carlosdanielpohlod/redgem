#pragma once

#include <optional>
#include <string>

#include "audio/audio.hpp"
#include "core/math.hpp"
#include "editor/console_log.hpp"
#include "editor/editor_session.hpp"
#include "editor/project_files.hpp"
#include "editor/script_workspace.hpp"

namespace redgem::ui {

// What every panel works with.
struct PanelContext {
  EditorSession& session;
  ScriptWorkspace& scripts;
  const ProjectFiles& files;
  Vec3 spawn_point;              // where new objects appear (in front of the editor camera)
  AudioSystem* audio = nullptr;  // sound previews in the Project panel
  bool files_changed = false;    // set by panels that create files: the editor rescans the game
};

// Tree of scene objects with search. Click selects, double click asks to frame it, drag to
// reparent, right click for actions; scripts and models can be dropped on it.
struct HierarchyResult {
  bool frame_requested = false;
};
HierarchyResult draw_hierarchy(PanelContext& context);

// The selection as component cards (Transform, Mesh & material, Light, Camera, one card per
// script, Physics, Custom properties) and an Add Component button.
void draw_inspector(PanelContext& context);

// The game's files: scripts (double click edits, drag onto objects), models (double click or
// drag to add to the scene), sounds (double click plays) and scenes.
void draw_project(PanelContext& context);

void draw_console(ConsoleLog& console);

// One code editor tab per open script (dockable: drag tabs to split, like a code editor). Ctrl+S
// saves with a syntax check. New tabs dock into `new_tab_dock`.
struct ScriptTabsResult {
  std::optional<std::string> split_right;  // window name of a tab to move into a split on its right
  unsigned int focused_dock = 0;           // dock node of the focused script tab, if any
};
ScriptTabsResult draw_script_tabs(ScriptWorkspace& scripts, unsigned int new_tab_dock);
// Window name of a script's tab.
std::string script_window_name(const std::string& file_name);

// Menu items that create objects (GameObject menu, hierarchy "+" and context menus).
void draw_create_menu(PanelContext& context);

// Panels remember things between frames (the hierarchy's search, half-typed names, open
// dialogs). Call when another game is opened so nothing carries over.
void reset_panel_state();
void reset_hierarchy_state();
void reset_inspector_state();
void reset_project_state();
void reset_script_tabs_state();

// Opens a script in its own tab (or brings the tab to the front).
void open_script(ScriptWorkspace& scripts, const std::string& file_name);

}  // namespace redgem::ui
