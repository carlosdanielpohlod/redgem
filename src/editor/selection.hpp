#pragma once

// What is selected in the editor: one object, several, or none. Clicks in the Hierarchy and the
// Scene view change it (a plain click picks one, Ctrl adds or takes away one, Shift picks a run of
// Hierarchy rows), as does a rectangle dragged over the Scene view.

#include <cstddef>
#include <vector>

#include "scene/scene.hpp"
#include "scene/scene_desc.hpp"

namespace redgem {

// What a click does to the selection, by the keys held with it.
enum class ClickMode {
  Replace,  // the clicked object alone
  Toggle,   // Ctrl: added, or taken away when it was selected
  Range,    // Shift: every row from the last one clicked to this one
};
ClickMode click_mode(bool ctrl, bool shift);
// The same for a click in the Scene view, which lists no rows: Ctrl or Shift adds the object, or
// takes it away.
ClickMode view_click_mode(bool ctrl, bool shift);

// What a rectangle dragged over the Scene view does: the objects in it are the selection, or
// (with Ctrl or Shift held) are added to it.
enum class BoxMode { Replace, Add };
BoxMode box_mode(bool ctrl, bool shift);

class Selection {
 public:
  // The selected objects, in the order they were picked.
  const std::vector<ObjectId>& ids() const { return ids_; }
  bool empty() const { return ids_.empty(); }
  std::size_t size() const { return ids_.size(); }
  bool several() const { return ids_.size() > 1; }
  bool contains(ObjectId id) const;
  // The first one picked (kNoObject when nothing is): what the Inspector shows of one object,
  // and the group's pivot when it turns around its first object.
  ObjectId primary() const { return ids_.empty() ? kNoObject : ids_.front(); }

  // Only `id` (nothing for kNoObject).
  void set(ObjectId id);
  // Exactly these, in this order (repeated ones and kNoObject left out).
  void set_all(const std::vector<ObjectId>& ids);
  void add_all(const std::vector<ObjectId>& ids);
  void toggle(ObjectId id);
  // Every object of `order` (the rows as the Hierarchy lists them) from the last one clicked to
  // `id`. Without such a row to start from, `id` alone.
  void extend_to(ObjectId id, const std::vector<ObjectId>& order);
  // What a click on `id` does with `mode`.
  void click(ObjectId id, ClickMode mode, const std::vector<ObjectId>& order);
  void clear() { set(kNoObject); }
  // Leaves out the objects that are not one of `count` any more (after a removal).
  void keep_below(int count);

 private:
  void add(ObjectId id);

  std::vector<ObjectId> ids_;
  ObjectId anchor_ = kNoObject;  // the last object clicked without Shift: where a run of rows starts
};

// The selected objects none of whose ancestors is selected too: what moves, is copied, duplicated
// or deleted (a child goes along with its parent anyway), in the order they were picked.
std::vector<int> top_level_of(const std::vector<int>& ids, const std::vector<NodeDesc>& nodes);

}  // namespace redgem
