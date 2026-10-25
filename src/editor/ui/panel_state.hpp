#pragma once

#include <map>
#include <string>
#include <vector>

#include "core/log.hpp"
#include "editor/edit_stream.hpp"
#include "editor/inspector_logic.hpp"
#include "editor/selection.hpp"

// What the panels remember between frames: the hierarchy's search, half-typed names, open
// dialogs, which messages the console lists. One of these belongs to each editor, so nothing
// carries over from one game to the next, nor from one editor to another.
namespace redgem::ui {

struct HierarchyState {
  std::string filter;           // the search field
  std::vector<int> rows;        // the objects of the rows listed last frame, top to bottom (for Shift-click)
  std::vector<int> rows_drawn;  // the same, as this frame lists them
  GroupPress press;             // a press on a row of a selection of several, until the button is up
};

// What a text field of the Inspector is doing.
struct TextFieldState {
  std::string buffer;
  std::string original;
  int object = kNoIndex;  // the object the field showed when typing started
  bool editing = false;
};

struct InspectorState {
  EditStream edits;                                    // the drag in progress, as one undo step
  std::map<unsigned int, TextFieldState> text_fields;  // by widget id
  std::string new_script_name;                         // Add Component > Script
  std::string new_property_key;                        // Add Component > Property
  int new_property_type = 0;                           // of the kinds the menu offers
  inspector_logic::GroupTargets group_tag;             // whom the Tag of several objects is typed for
};

// The folders of a game the Project panel lists.
enum class ProjectFolder { Scripts, Models, Sounds, Scenes, Images };

struct ProjectState {
  std::string new_script_name;
  ProjectFolder folder = ProjectFolder::Scripts;  // the one whose files are shown
};

// Which kinds of messages the Console lists (the buttons at its top toggle them).
struct ConsoleFilters {
  bool info = true;
  bool warnings = true;
  bool errors = true;

  bool shows(log::Level level) const {
    if (level == log::Level::Error) return errors;
    return level == log::Level::Warning ? warnings : info;
  }
};

struct ConsoleState {
  ConsoleFilters filters;
};

struct ScriptTabsState {
  std::string pending_close;  // a tab closed with unsaved changes, waiting for the dialog
};

struct PanelState {
  HierarchyState hierarchy;
  InspectorState inspector;
  ProjectState project;
  ConsoleState console;
  ScriptTabsState script_tabs;
};

// Another scene opens: what the panels held of the last one's objects goes (the search, half-typed
// names, a streamed edit, which would land on the new scene's objects), without touching the new
// scene. What is the game's (the Project panel, the Console, the script tabs) stays.
inline void forget_scene(PanelState& state) {
  state.hierarchy = HierarchyState{};
  state.inspector = InspectorState{};
}

}  // namespace redgem::ui
