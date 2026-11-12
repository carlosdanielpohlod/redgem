#include "navigation/nav_path.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <optional>
#include <queue>
#include <unordered_map>

namespace redgem {
namespace {

using Node = NavGrid::Node;

// Where a line between two cells' middles crosses a cell's edge, in half cells (the middles are
// half a cell from the edges): its k-th crossing is at 2k + 1 of them.
constexpr int kHalvesPerCell = 2;
// How much longer a diagonal step is than a straight one, per straight length: sqrt(2) - 1.
constexpr float kDiagonalSaving = 0.41421356f;
// A path of this many nodes or fewer has nothing to pull straight.
constexpr std::size_t kShortestPullable = 2;

// What a search knows of a node it reached: how far it is from the start, the node it is reached
// from, and whether the shortest way there is known.
struct Visit {
  float cost = 0.0f;
  Node parent = 0;
  bool closed = false;
};

// A node to look at, with its cost from the start plus a guess of the rest of the way.
struct Open {
  float estimate = 0.0f;
  Node node = 0;
  bool operator>(const Open& other) const { return estimate > other.estimate; }
};

bool improves(const Visit& visit, float cost) { return !visit.closed && cost < visit.cost; }

bool worth_reaching(bool first_time, const Visit& visit, float cost) { return first_time || improves(visit, cost); }

class Search {
 public:
  Search(const NavGrid& grid, Node goal) : grid_(grid), goal_(goal), goal_position_(grid.position(goal)) {}
  NavGridPath run(Node start, std::size_t budget);

 private:
  float guess(Node node) const;
  void reach(Node node, Node parent, float cost);
  void expand(Node node);
  std::optional<Node> next_open();
  std::vector<Node> chain_to(Node node) const;
  bool ends_at(Node node, std::size_t budget, NavGridPath& path) const;

  const NavGrid& grid_;
  Node goal_;
  Vec3 goal_position_;
  std::unordered_map<Node, Visit> visits_;
  std::priority_queue<Open, std::vector<Open>, std::greater<Open>> open_;
};

// The octile distance to the goal: the length of the shortest way there on open, flat ground,
// diagonals included. A path is never shorter, so the search still finds the shortest one.
float Search::guess(Node node) const {
  const Vec3 position = grid_.position(node);
  const float across = std::fabs(goal_position_.x - position.x);
  const float along = std::fabs(goal_position_.z - position.z);
  return std::max(across, along) + kDiagonalSaving * std::min(across, along);
}

void Search::reach(Node node, Node parent, float cost) {
  const auto [entry, first_time] = visits_.try_emplace(node, Visit{cost, parent, false});
  if (!worth_reaching(first_time, entry->second, cost)) return;
  entry->second = Visit{cost, parent, false};
  open_.push({cost + guess(node), node});
}

void Search::expand(Node node) {
  Visit& visit = visits_.at(node);
  visit.closed = true;
  const float cost = visit.cost;  // (reaching others may move the map's entries)
  NavGrid::Links links;
  const std::size_t count = grid_.neighbours(node, links);
  for (std::size_t index = 0; index < count; ++index) reach(links[index].node, node, cost + links[index].cost);
}

// The open node nearest the goal by its estimate, or none when every way was tried.
std::optional<Node> Search::next_open() {
  while (!open_.empty()) {
    const Node node = open_.top().node;
    open_.pop();
    if (!visits_.at(node).closed) return node;  // (a node reached again later is in the queue twice)
  }
  return std::nullopt;
}

std::vector<Node> Search::chain_to(Node node) const {
  std::vector<Node> chain{node};
  for (Node parent = visits_.at(node).parent; parent != chain.back(); parent = visits_.at(parent).parent) {
    chain.push_back(parent);
  }
  std::reverse(chain.begin(), chain.end());
  return chain;
}

// Whether the search ends at a node it takes from the queue: at the goal (a path), or with its
// budget spent.
bool Search::ends_at(Node node, std::size_t budget, NavGridPath& path) const {
  if (node == goal_) {
    path.status = NavPathStatus::Found;
    path.nodes = chain_to(node);
    return true;
  }
  if (path.searched < budget) return false;
  path.status = NavPathStatus::OutOfBudget;
  return true;
}

NavGridPath Search::run(Node start, std::size_t budget) {
  NavGridPath path;
  reach(start, start, 0.0f);
  for (std::optional<Node> node = next_open(); node.has_value(); node = next_open()) {
    if (ends_at(*node, budget, path)) return path;
    ++path.searched;
    expand(*node);
  }
  return path;  // every way tried: unreachable
}

int sign_of(int value) { return (value > 0) - (value < 0); }

// Whether a node is a step from both of two others.
bool joined_from_both(const NavGrid& grid, Node one, Node other, Node node) {
  return grid.joins(one, node) && grid.joins(other, node);
}

// A walk along the straight line from one node's middle to another's, cell by cell: every cell
// the line crosses, in order (a supercover of the line). Where it goes through a corner, both
// cells beside it must lead to the one across it, as a diagonal step of a search must.
class StraightWalk {
 public:
  StraightWalk(const NavGrid& grid, Node from, Node to);
  bool run();

 private:
  // How far along the line the next crossing of a column's edge (or a row's) is, in a measure the
  // two can be compared in: whole numbers, so that a corner is exactly both.
  long long next_x_crossing() const { return static_cast<long long>(kHalvesPerCell * crossed_x_ + 1) * across_z_; }
  long long next_z_crossing() const { return static_cast<long long>(kHalvesPerCell * crossed_z_ + 1) * across_x_; }
  bool x_left() const { return crossed_x_ < across_x_; }
  bool z_left() const { return crossed_z_ < across_z_; }
  bool crossings_left() const { return x_left() || z_left(); }
  bool x_first() const { return x_left() && (!z_left() || next_x_crossing() < next_z_crossing()); }
  bool z_first() const { return z_left() && (!x_left() || next_z_crossing() < next_x_crossing()); }
  bool advance();
  bool step(int dx, int dz);
  bool through_corner();
  std::optional<Node> onto(int dx, int dz) const;

  const NavGrid& grid_;
  Node at_;
  Node to_;
  NavCell cell_;
  int step_x_ = 0;
  int step_z_ = 0;
  int across_x_ = 0;  // columns the line crosses into, along x
  int across_z_ = 0;
  int crossed_x_ = 0;
  int crossed_z_ = 0;
};

StraightWalk::StraightWalk(const NavGrid& grid, Node from, Node to)
    : grid_(grid), at_(from), to_(to), cell_(grid.cell_of_column(grid.column_of(from))) {
  const NavCell end = grid.cell_of_column(grid.column_of(to));
  step_x_ = sign_of(end.x - cell_.x);
  step_z_ = sign_of(end.z - cell_.z);
  across_x_ = std::abs(end.x - cell_.x);
  across_z_ = std::abs(end.z - cell_.z);
}

// The node a character on the walk's node steps onto in the cell beside, if any.
std::optional<Node> StraightWalk::onto(int dx, int dz) const {
  const std::optional<int> column = grid_.column_of_cell({cell_.x + dx, cell_.z + dz});
  if (!column) return std::nullopt;
  return grid_.step_to(at_, *column);
}

bool StraightWalk::step(int dx, int dz) {
  const std::optional<Node> next = onto(dx, dz);
  if (!next) return false;
  at_ = *next;
  cell_ = {cell_.x + dx, cell_.z + dz};
  return true;
}

bool StraightWalk::through_corner() {
  const std::optional<Node> beside_x = onto(step_x_, 0);
  const std::optional<Node> beside_z = onto(0, step_z_);
  const std::optional<Node> across = onto(step_x_, step_z_);
  if (!beside_x) return false;
  if (!beside_z) return false;
  if (!across) return false;
  if (!joined_from_both(grid_, *beside_x, *beside_z, *across)) return false;
  return step(step_x_, step_z_);
}

bool StraightWalk::advance() {
  if (x_first()) {
    ++crossed_x_;
    return step(step_x_, 0);
  }
  if (z_first()) {
    ++crossed_z_;
    return step(0, step_z_);
  }
  ++crossed_x_;
  ++crossed_z_;
  return through_corner();
}

bool StraightWalk::run() {
  while (crossings_left()) {
    if (!advance()) return false;
  }
  return at_ == to_;
}

bool straight_fits(const NavGrid& grid, const std::vector<Node>& nodes, std::size_t anchor, std::size_t next) {
  return next - anchor <= nav::kMaxStraightCells && walks_straight(grid, nodes[anchor], nodes[next]);
}

}  // namespace

NavGridPath find_nav_path(const NavGrid& grid, Node start, Node goal, std::size_t budget) {
  return Search(grid, goal).run(start, budget);
}

bool walks_straight(const NavGrid& grid, Node from, Node to) { return StraightWalk(grid, from, to).run(); }

std::vector<Node> pull_straight(const NavGrid& grid, const std::vector<Node>& nodes) {
  if (nodes.size() <= kShortestPullable) return nodes;
  std::vector<Node> pulled{nodes.front()};
  std::size_t anchor = 0;
  for (std::size_t next = anchor + kShortestPullable; next < nodes.size(); ++next) {
    if (straight_fits(grid, nodes, anchor, next)) continue;
    anchor = next - 1;
    pulled.push_back(nodes[anchor]);
  }
  pulled.push_back(nodes.back());
  return pulled;
}

float nav_path_length(const NavGrid& grid, const std::vector<Node>& nodes) {
  float total = 0.0f;
  for (std::size_t index = 1; index < nodes.size(); ++index) {
    total += length(grid.position(nodes[index]) - grid.position(nodes[index - 1]));
  }
  return total;
}

}  // namespace redgem
