#pragma once

// The rows of the Hierarchy: what each shows of its object, and what the mouse does on it.

#include <string>

#include "editor/ui/panels.hpp"

namespace redgem::ui::hierarchy {

// Edits are collected while the tree is drawn and applied after it (they change indices).
struct Pending {
  enum class Kind { None, Duplicate, Delete, Reparent, AttachScript, ImportModel };
  Kind kind = Kind::None;
  int target = kNoIndex;
  int parent = kNoIndex;
  std::string file;
};

// What a row shows of its object (copied: the row's menu can create objects, which reallocates
// the node list).
struct RowLabel {
  std::string name;
  const char* icon = "";
  bool template_object = false;
  bool hidden = false;
  bool has_script = false;

  // Templates and hidden objects are named in the muted color.
  bool dimmed() const { return hidden || template_object; }
};
RowLabel label_of(const NodeDesc& node);

// Accepts nodes, scripts and models dropped on the last item (`target` kNoIndex = top level).
void accept_drops(int target, Pending& pending);

// One row: a tree node (with ImGui's `flags`) that selects, frames, drags, takes drops and opens
// the object's menu. True when it is open: its children follow.
bool draw_row(PanelContext& context, int index, int flags, HierarchyResult& result, Pending& pending);

}  // namespace redgem::ui::hierarchy
