#include "navigation/nav_path.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <queue>
#include <unordered_map>

namespace redgem {
namespace {

using Node = NavGrid::Node;

// How far apart, in cells, the points a straight walk is checked at are.
constexpr float kStraightSampleSpacing = 0.25f;
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

// Where a walk from `at` is when it gets over (x, z): the same node within its column, the node of
// the column it stepped onto, or none off the walkable cells.
std::optional<Node> step_along(const NavGrid& grid, Node at, float x, float z) {
  const std::optional<int> column = grid.column_at(x, z);
  if (!column) return std::nullopt;
  if (*column == grid.column_of(at)) return at;
  return grid.step_to(at, *column);
}

bool straight_fits(const NavGrid& grid, const std::vector<Node>& nodes, std::size_t anchor, std::size_t next) {
  return next - anchor <= nav::kMaxStraightCells && walks_straight(grid, nodes[anchor], nodes[next]);
}

}  // namespace

NavGridPath find_nav_path(const NavGrid& grid, Node start, Node goal, std::size_t budget) {
  return Search(grid, goal).run(start, budget);
}

bool walks_straight(const NavGrid& grid, Node from, Node to) {
  const Vec3 a = grid.position(from);
  const Vec3 b = grid.position(to);
  const float across = std::hypot(b.x - a.x, b.z - a.z);
  const int samples =
      std::max(1, static_cast<int>(std::ceil(across / (grid.shape().cell_size * kStraightSampleSpacing))));
  Node at = from;
  for (int sample = 1; sample <= samples; ++sample) {
    const float share = static_cast<float>(sample) / static_cast<float>(samples);
    const std::optional<Node> next = step_along(grid, at, a.x + (b.x - a.x) * share, a.z + (b.z - a.z) * share);
    if (!next) return false;
    at = *next;
  }
  return at == to;
}

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
