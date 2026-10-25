#include "editor/selection.hpp"

#include <algorithm>

namespace redgem {
namespace {

std::ptrdiff_t position_of(const std::vector<ObjectId>& order, ObjectId id) {
  return std::find(order.begin(), order.end(), id) - order.begin();
}

bool listed(const std::vector<ObjectId>& order, ObjectId id) {
  return std::find(order.begin(), order.end(), id) != order.end();
}

// A run of rows can start from the last one clicked: there is one, and the Hierarchy lists it.
bool run_can_start(ObjectId anchor, const std::vector<ObjectId>& order) {
  return anchor != kNoObject && listed(order, anchor);
}

// Whether one of the node's ancestors is among `ids`.
bool has_selected_ancestor(int node, const std::vector<int>& ids, const std::vector<NodeDesc>& nodes) {
  std::size_t steps = 0;  // never more than there are nodes, whatever their links say
  for (int up = nodes[node].parent; up != kNoIndex && steps < nodes.size(); up = nodes[up].parent, ++steps) {
    if (std::find(ids.begin(), ids.end(), up) != ids.end()) return true;
  }
  return false;
}

// In the Scene view, Ctrl and Shift both keep what is selected.
bool adds_to_selection(bool ctrl, bool shift) { return ctrl || shift; }

}  // namespace

ClickMode click_mode(bool ctrl, bool shift) {
  if (shift) return ClickMode::Range;
  return ctrl ? ClickMode::Toggle : ClickMode::Replace;
}

ClickMode view_click_mode(bool ctrl, bool shift) {
  return adds_to_selection(ctrl, shift) ? ClickMode::Toggle : ClickMode::Replace;
}

BoxMode box_mode(bool ctrl, bool shift) { return adds_to_selection(ctrl, shift) ? BoxMode::Add : BoxMode::Replace; }

bool Selection::contains(ObjectId id) const { return std::find(ids_.begin(), ids_.end(), id) != ids_.end(); }

void Selection::add(ObjectId id) {
  if (id != kNoObject && !contains(id)) ids_.push_back(id);
}

void Selection::set(ObjectId id) {
  ids_.clear();
  add(id);
  anchor_ = id;
}

void Selection::set_all(const std::vector<ObjectId>& ids) {
  ids_.clear();
  add_all(ids);
  anchor_ = primary();
}

void Selection::add_all(const std::vector<ObjectId>& ids) {
  for (const ObjectId id : ids) add(id);
}

void Selection::toggle(ObjectId id) {
  anchor_ = id;
  if (!contains(id)) return add(id);
  ids_.erase(std::find(ids_.begin(), ids_.end(), id));
}

void Selection::extend_to(ObjectId id, const std::vector<ObjectId>& order) {
  if (!run_can_start(anchor_, order) || !listed(order, id)) return set(id);
  const std::ptrdiff_t from = position_of(order, anchor_);
  const std::ptrdiff_t to = position_of(order, id);
  const ObjectId anchor = anchor_;
  ids_.clear();
  for (std::ptrdiff_t row = std::min(from, to); row <= std::max(from, to); ++row) add(order[row]);
  anchor_ = anchor;  // the next Shift-click starts from the same row
}

void Selection::click(ObjectId id, ClickMode mode, const std::vector<ObjectId>& order) {
  if (mode == ClickMode::Toggle) return toggle(id);
  if (mode == ClickMode::Range) return extend_to(id, order);
  set(id);
}

void Selection::keep_below(int count) {
  ids_.erase(std::remove_if(ids_.begin(), ids_.end(), [count](ObjectId id) { return id >= count; }), ids_.end());
  if (anchor_ >= count) anchor_ = kNoObject;
}

std::vector<int> top_level_of(const std::vector<int>& ids, const std::vector<NodeDesc>& nodes) {
  std::vector<int> top;
  for (const int id : ids) {
    if (!has_selected_ancestor(id, ids, nodes)) top.push_back(id);
  }
  return top;
}

}  // namespace redgem
