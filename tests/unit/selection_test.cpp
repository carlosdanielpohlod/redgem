#include <doctest/doctest.h>

#include <vector>

#include "editor/node_tree.hpp"
#include "editor/selection.hpp"

using namespace redgem;

namespace {

using Ids = std::vector<ObjectId>;

const Ids kRows = {4, 2, 7, 1, 9};  // the Hierarchy's rows, top to bottom

// A parent (0) with two children (1, 2), the second with a child (3), and an object of its own (4).
std::vector<NodeDesc> family() {
  std::vector<NodeDesc> nodes = {node_tree::named_node("Parent", kNoIndex), node_tree::named_node("A", 0),
                                 node_tree::named_node("B", 0), node_tree::named_node("C", 2),
                                 node_tree::named_node("Alone", kNoIndex)};
  nodes[0].children = {1, 2};
  nodes[2].children = {3};
  return nodes;
}

}  // namespace

TEST_CASE("the keys held with a click say what it does to the selection") {
  CHECK(click_mode(false, false) == ClickMode::Replace);
  CHECK(click_mode(true, false) == ClickMode::Toggle);
  CHECK(click_mode(false, true) == ClickMode::Range);
  CHECK(click_mode(true, true) == ClickMode::Range);
  // The Scene view lists no rows: Shift adds there, as Ctrl does.
  CHECK(view_click_mode(false, false) == ClickMode::Replace);
  CHECK(view_click_mode(true, false) == ClickMode::Toggle);
  CHECK(view_click_mode(false, true) == ClickMode::Toggle);
  CHECK(box_mode(false, false) == BoxMode::Replace);
  CHECK(box_mode(true, false) == BoxMode::Add);
  CHECK(box_mode(false, true) == BoxMode::Add);
}

TEST_CASE("a plain click selects one object, Ctrl adds and takes away, in the order picked") {
  Selection selection;
  CHECK(selection.empty());
  CHECK(selection.primary() == kNoObject);
  selection.click(7, ClickMode::Replace, kRows);
  CHECK(selection.ids() == Ids{7});
  selection.click(2, ClickMode::Toggle, kRows);
  selection.click(9, ClickMode::Toggle, kRows);
  CHECK(selection.ids() == Ids{7, 2, 9});
  CHECK(selection.primary() == 7);
  CHECK(selection.several());
  selection.click(2, ClickMode::Toggle, kRows);
  CHECK(selection.ids() == Ids{7, 9});
  selection.click(1, ClickMode::Replace, kRows);
  CHECK(selection.ids() == Ids{1});
  selection.set(kNoObject);
  CHECK(selection.empty());
}

TEST_CASE("Shift selects the run of rows from the last one clicked, either way") {
  Selection selection;
  selection.click(2, ClickMode::Replace, kRows);
  selection.click(1, ClickMode::Range, kRows);
  CHECK(selection.ids() == Ids{2, 7, 1});
  // The run starts from the same row again, upwards this time.
  selection.click(4, ClickMode::Range, kRows);
  CHECK(selection.ids() == Ids{4, 2});
  // A Ctrl-click moves where the next run starts.
  selection.click(9, ClickMode::Toggle, kRows);
  selection.click(7, ClickMode::Range, kRows);
  CHECK(selection.ids() == Ids{7, 1, 9});
}

TEST_CASE("Shift without a row to start from selects the clicked object alone") {
  Selection selection;
  selection.click(7, ClickMode::Range, kRows);
  CHECK(selection.ids() == Ids{7});
  selection.click(3, ClickMode::Range, kRows);  // a row the Hierarchy does not list (filtered out)
  CHECK(selection.ids() == Ids{3});
}

TEST_CASE("setting and adding many leave out repeats and nothing") {
  Selection selection;
  selection.set_all({3, kNoObject, 5, 3});
  CHECK(selection.ids() == Ids{3, 5});
  selection.add_all({5, 8});
  CHECK(selection.ids() == Ids{3, 5, 8});
  CHECK(selection.contains(8));
  CHECK_FALSE(selection.contains(4));
}

TEST_CASE("objects past the end of the scene leave the selection") {
  Selection selection;
  selection.set_all({1, 6, 3, 9});
  selection.keep_below(4);
  CHECK(selection.ids() == Ids{1, 3});
}

TEST_CASE("the top level of a selection leaves out what goes along with a selected ancestor") {
  const std::vector<NodeDesc> nodes = family();
  CHECK(top_level_of({3, 0, 4}, nodes) == std::vector<int>{0, 4});
  CHECK(top_level_of({1, 3}, nodes) == std::vector<int>{1, 3});
  CHECK(top_level_of({3, 2}, nodes) == std::vector<int>{2});
  CHECK(top_level_of({}, nodes).empty());
}
