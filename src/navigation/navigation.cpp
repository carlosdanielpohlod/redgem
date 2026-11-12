#include "navigation/navigation.hpp"

#include <utility>

#include "core/stopwatch.hpp"
#include "navigation/nav_path.hpp"
#include "navigation/nav_physics_probe.hpp"
#include "navigation/nav_queries.hpp"

namespace redgem {
namespace {

// How far above the level's highest point (and below its lowest) the rays of a bake start (and end).
constexpr float kRayHeadroom = 1.0f;
constexpr double kMillisecondsPerSecond = 1000.0;

std::vector<Vec3> positions(const NavGrid& grid, const std::vector<NavGrid::Node>& nodes) {
  std::vector<Vec3> points;
  points.reserve(nodes.size());
  for (const NavGrid::Node node : nodes) points.push_back(grid.position(node));
  return points;
}

}  // namespace

const NavBakeReport& Navigation::bake(const NavLevel& level) {
  const double started = steady_seconds();
  const Bounds area = static_level_bounds(level.scene, level.desc, level.physics);
  const NavGridShape shape = nav_grid_shape(area, settings_);
  const PhysicsNavProbe probe(level.physics, settings_, area.max.y + kRayHeadroom, area.min.y - kRayHeadroom);
  NavGrid baked = bake_nav_grid(shape, settings_, probe);
  report_.columns = shape.column_count();
  report_.cells = baked.node_count();
  grid_ = std::move(baked);
  report_.cell_size = shape.cell_size;
  report_.cells_enlarged = shape.cell_size > settings_.cell_size;
  report_.milliseconds = (steady_seconds() - started) * kMillisecondsPerSecond;
  return report_;
}

const NavGrid& Navigation::grid() const {
  static const NavGrid kNothing;
  return grid_ ? *grid_ : kNothing;
}

std::optional<std::vector<Vec3>> Navigation::path(Vec3 from, Vec3 to) const {
  const NavGrid& walked = grid();
  const std::optional<NavGrid::Node> start = nearest_nav_node(walked, from);
  const std::optional<NavGrid::Node> goal = nearest_nav_node(walked, to);
  if (!start) return std::nullopt;
  if (!goal) return std::nullopt;
  const NavGridPath found = find_nav_path(walked, *start, *goal);
  if (found.status != NavPathStatus::Found) return std::nullopt;
  return positions(walked, pull_straight(walked, found.nodes));
}

std::optional<Vec3> Navigation::nearest(Vec3 point) const {
  const std::optional<NavGrid::Node> node = nearest_nav_node(grid(), point);
  if (!node) return std::nullopt;
  return grid().position(*node);
}

bool Navigation::walkable(Vec3 point) const { return on_nav_grid(grid(), point, settings_.agent_height); }

}  // namespace redgem
