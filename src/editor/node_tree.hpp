#pragma once

// What is decided about a scene's tree of nodes without a document: subtrees, placements in the
// world, the order nodes are kept in, and the names copies get.

#include <string>
#include <vector>

#include "scene/scene_desc.hpp"
#include "scene/transform.hpp"

namespace redgem::node_tree {

// "Crate" and 1 -> "Crate.001".
std::string numbered_name(const std::string& base, int number);
// A node called `name` under `parent` (kNoIndex = top level), with nothing else yet.
NodeDesc named_node(const std::string& name, int parent);
// Indices of `index` and all its descendants, parents first.
std::vector<int> subtree(const std::vector<NodeDesc>& nodes, int index);
// The same for several nodes, one subtree after the other (none of `roots` under another one).
std::vector<int> subtrees(const std::vector<NodeDesc>& nodes, const std::vector<int>& roots);
// Where a node is in the world, following its parents.
Transform world_of(const std::vector<NodeDesc>& nodes, int index);
// Where a node has to be placed under `parent` (kNoIndex = top level) to stay where it is in the world.
Transform local_under(const std::vector<NodeDesc>& nodes, int index, int parent);
// Puts `child` under `parent` (kNoIndex = top level), placed at `local`.
void move_under(std::vector<NodeDesc>& nodes, int child, int parent, const Transform& local);
// The indices 0..count-1 in order without `removed`, and with `last` moved to the end.
std::vector<int> order_without(int count, const std::vector<int>& removed);
std::vector<int> order_with_last(int count, const std::vector<int>& last);
// For each of `count` old indices, where `order` puts it (kNoIndex when it is left out).
std::vector<int> positions_in(const std::vector<int>& order, std::size_t count);
// The nodes listed by `order` (old indices), with parents and children renumbered to match.
// Children left out of the order are dropped from their parent.
std::vector<NodeDesc> nodes_in_order(const std::vector<NodeDesc>& nodes, const std::vector<int>& order);

}  // namespace redgem::node_tree
