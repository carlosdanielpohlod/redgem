#pragma once

// MCP scene_edit's changes, on an open scene: the same for the saved files and for the open editor.
//
//   set        words (assignments) on one object or on several (`objects`)
//   add        a new object named `object`: empty, of a `kind` (a shape, a light, a camera, a
//              terrain, the environment), or a copy of `from`; optionally under `parent`
//   duplicate  copies of one object or several, each with its children, the words set on each copy
//   group      a new empty object named `object`, the `objects` moved under it (world placement kept)
//   remove     one object or several, with their children (`confirm` must be true)
//
// Objects are named as scene_get names them (#id, path or unique name), and found again by their
// persistent id after an edit that renumbers them.

#include <string>
#include <vector>

#include "project/mcp_protocol.hpp"
#include "project/scene_assignments.hpp"

namespace redgem::tools {

// What a call leaves: the objects to show and to select, and its answer (a line per object).
struct EditOutcome {
  std::vector<int> objects;
  std::string text;
};

// Runs one scene_edit call (its arguments as the tool's schema gives them) on `edit`'s document,
// as one undo step. A wrong call throws std::invalid_argument with what to change, and changes
// nothing: no object, no undo step, the redo steps there were kept (except after a copy or a new
// shape, light, camera or terrain, which clears them as the editor's own do).
EditOutcome edit_scene(SceneEdit& edit, const mcp::Json& args);

}  // namespace redgem::tools
