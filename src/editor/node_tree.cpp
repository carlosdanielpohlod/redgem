#include "editor/node_tree.hpp"

#include <algorithm>
#include <cstdio>

namespace redgem::node_tree {
namespace {

constexpr int kSuffixDigits = 3;                // digits of the number a copy's name ends with
constexpr int kSuffixBufferSize = 16;           // characters: room for a dot and any int
constexpr const char* kSuffixFormat = ".%0*d";  // a dot and the number, zero-padded to those digits

// Where each of `children` is after a reordering, without the ones it left out.
std::vector<int> kept_children(const std::vector<int>& children, const std::vector<int>& position) {
  std::vector<int> kept;
  for (const int child : children) {
    if (position[child] != kNoIndex) kept.push_back(position[child]);
  }
  return kept;
}

void detach_from_parent(std::vector<NodeDesc>& nodes, int child) {
  const int parent = nodes[child].parent;
  if (parent == kNoIndex) return;
  std::vector<int>& siblings = nodes[parent].children;
  siblings.erase(std::remove(siblings.begin(), siblings.end(), child), siblings.end());
}

}  // namespace

std::string numbered_name(const std::string& base, int number) {
  char buffer[kSuffixBufferSize];
  std::snprintf(buffer, sizeof(buffer), kSuffixFormat, kSuffixDigits, number);
  return base + buffer;
}

NodeDesc named_node(const std::string& name, int parent) {
  NodeDesc node;
  node.name = name;
  node.parent = parent;
  return node;
}

// (A list of nodes still to visit, not a function calling itself: however deep a scene is nested,
// walking it takes no more stack. Never more nodes than there are, whatever their links say.)
std::vector<int> subtree(const std::vector<NodeDesc>& nodes, int index) {
  std::vector<int> out;
  std::vector<int> pending = {index};
  const auto more_to_visit = [&] { return !pending.empty() && out.size() < nodes.size(); };
  while (more_to_visit()) {
    const int next = pending.back();
    pending.pop_back();
    out.push_back(next);
    const std::vector<int>& children = nodes[next].children;
    pending.insert(pending.end(), children.rbegin(), children.rend());  // the first child comes out first
  }
  return out;
}

// (Up from the node to its top-level ancestor, then the placements combined from the top down.)
Transform world_of(const std::vector<NodeDesc>& nodes, int index) {
  std::vector<int> chain;
  const auto more_above = [&](int node) { return node != kNoIndex && chain.size() < nodes.size(); };
  for (int node = index; more_above(node); node = nodes[node].parent) chain.push_back(node);
  Transform world = nodes[chain.back()].local;
  for (auto node = chain.rbegin() + 1; node != chain.rend(); ++node) world = combine(world, nodes[*node].local);
  return world;
}

Transform local_under(const std::vector<NodeDesc>& nodes, int index, int parent) {
  const Transform world = world_of(nodes, index);
  return parent == kNoIndex ? world : relative_to(world_of(nodes, parent), world);
}

void move_under(std::vector<NodeDesc>& nodes, int child, int parent, const Transform& local) {
  detach_from_parent(nodes, child);
  nodes[child].parent = parent;
  if (parent != kNoIndex) nodes[parent].children.push_back(child);
  nodes[child].local = local;
}

std::vector<int> order_without(int count, const std::vector<int>& removed) {
  std::vector<int> order;
  for (int i = 0; i < count; ++i) {
    if (std::find(removed.begin(), removed.end(), i) == removed.end()) order.push_back(i);
  }
  return order;
}

std::vector<int> order_with_last(int count, const std::vector<int>& last) {
  std::vector<int> order = order_without(count, last);
  order.insert(order.end(), last.begin(), last.end());
  return order;
}

std::vector<int> positions_in(const std::vector<int>& order, std::size_t count) {
  std::vector<int> position(count, kNoIndex);
  for (int i = 0; i < static_cast<int>(order.size()); ++i) position[order[i]] = i;
  return position;
}

std::vector<NodeDesc> nodes_in_order(const std::vector<NodeDesc>& nodes, const std::vector<int>& order) {
  const std::vector<int> position = positions_in(order, nodes.size());
  std::vector<NodeDesc> out;
  out.reserve(order.size());
  for (const int old : order) out.push_back(nodes[old]);
  for (NodeDesc& node : out) {
    if (node.parent != kNoIndex) node.parent = position[node.parent];
    node.children = kept_children(node.children, position);
  }
  return out;
}

}  // namespace redgem::node_tree
