// Baking navigation grids out of small levels made of static boxes in a physics world: a flat
// floor, walls, steps too high, slopes too steep, gaps, a bridge and a low ceiling. And what a bake
// and a search across a large level cost.

#include <doctest/doctest.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/config.hpp"
#include "navigation/nav_bake.hpp"
#include "navigation/nav_path.hpp"
#include "navigation/nav_physics_probe.hpp"
#include "navigation/nav_queries.hpp"
#include "physics/physics_config.hpp"
#include "physics/physics_world.hpp"

using namespace redgem;

namespace {

constexpr float kCellSize = 0.25f;
constexpr float kAgentRadius = 0.3f;
constexpr float kHeadroom = 1.8f;
constexpr float kRayTop = 10.0f;
constexpr float kRayBottom = -10.0f;
constexpr float kHeightTolerance = 0.02f;
constexpr Vec3 kFloorHalf{5, 0.5f, 5};
constexpr Vec3 kFloorCenter{0, -0.5f, 0};
constexpr Vec3 kWest{-3, 0, 0};
constexpr Vec3 kEast{3, 0, 0};
constexpr Vec3 kFarWest{-4, 0, -3};
constexpr Vec3 kFarEast{4, 0, -3};

// One static box after another: ids in the order they are added.
class Level {
 public:
  Level() : world_(create_jolt_physics_world({0.0f, config::kGravityY, 0.0f})) {}

  void add_box(Vec3 center, Vec3 half, Quat rotation = Quat::identity(), const char* body = "static") {
    BodyDesc desc;
    desc.object = next_++;
    desc.config = body_config_from_props({{"body", std::string(body)}});
    desc.world.position = center;
    desc.world.rotation = rotation;
    desc.bounds.include(-half);
    desc.bounds.include(half);
    REQUIRE_FALSE(world_->add_object(desc));
  }
  void add_floor() { add_box(kFloorCenter, kFloorHalf); }

  NavGrid bake(const Bounds& area = floor_area(), const NavSettings& settings = settings_for_tests()) const {
    const NavGridShape shape = nav_grid_shape(area, settings);
    const PhysicsNavProbe probe(*world_, settings, shape.cell_size, kRayTop, kRayBottom);
    return bake_nav_grid(shape, settings, probe);
  }

  static NavSettings settings_for_tests() {
    NavSettings settings;
    settings.cell_size = kCellSize;
    settings.agent_radius = kAgentRadius;
    return settings;
  }
  static Bounds floor_area() {
    Bounds area;
    area.include(kFloorCenter - kFloorHalf);
    area.include(kFloorCenter + kFloorHalf);
    return area;
  }

 private:
  std::unique_ptr<PhysicsWorld> world_;
  ObjectId next_ = 0;
};

NavGridPath path_between(const NavGrid& grid, Vec3 from, Vec3 to) {
  const std::optional<NavGrid::Node> start = nearest_nav_node(grid, from);
  const std::optional<NavGrid::Node> goal = nearest_nav_node(grid, to);
  REQUIRE(start.has_value());
  REQUIRE(goal.has_value());
  return find_nav_path(grid, *start, *goal);
}

Quat tilted(float degrees) { return quat_from_axis_angle({0, 0, 1}, degrees_to_radians(degrees)); }

// A ramp tilted up toward +x, `half_length` meters from its middle to either end; where its top
// face is `along` meters from the middle.
struct Ramp {
  Vec3 center;
  float degrees = 0.0f;
  static constexpr float kHalfThickness = 0.1f;
  static constexpr float kHalfLength = 2.0f;
  static constexpr float kHalfWidth = 1.5f;

  Vec3 half() const { return {kHalfLength, kHalfThickness, kHalfWidth}; }
  Vec3 face_at(float along) const {
    const Quat turn = tilted(degrees);
    return center + rotate(turn, Vec3{along, kHalfThickness, 0});
  }
};

bool any_cell_above(const NavGrid& grid, float height) {
  for (NavGrid::Node node = 0; node < grid.node_count(); ++node) {
    if (grid.height(node) > height) return true;
  }
  return false;
}

}  // namespace

TEST_CASE("every cell of a flat floor is walkable, at the floor's height") {
  Level level;
  level.add_floor();
  const NavGrid grid = level.bake();
  CHECK(grid.shape().columns == 40);
  CHECK(grid.shape().rows == 40);
  CHECK(grid.node_count() == static_cast<std::size_t>(grid.shape().column_count()));
  for (NavGrid::Node node = 0; node < grid.node_count(); ++node) {
    CHECK(grid.height(node) == doctest::Approx(0.0f).epsilon(kHeightTolerance));
  }
}

TEST_CASE("a wall leaves no cell under it or within a character's radius of it, and splits the floor") {
  Level level;
  level.add_floor();
  level.add_box({0, 1, 0}, {0.25f, 1, 5});  // across the whole floor
  const NavGrid grid = level.bake();
  CHECK_FALSE(on_nav_grid(grid, {0.0f, 0.0f, 0.0f}, kHeadroom));
  CHECK_FALSE(on_nav_grid(grid, {-0.4f, 0.0f, 0.0f}, kHeadroom));  // 0.15 m from its face
  CHECK(on_nav_grid(grid, {-0.9f, 0.0f, 0.0f}, kHeadroom));
  CHECK(path_between(grid, kWest, kEast).status == NavPathStatus::Unreachable);
}

TEST_CASE("a path goes through the gap of a wall") {
  Level level;
  level.add_floor();
  level.add_box({0, 1, -1.5f}, {0.25f, 1, 3.5f});  // from z = -5 to 2: the gap is past z = 2
  const NavGrid grid = level.bake();
  const NavGridPath path = path_between(grid, kFarWest, kFarEast);
  REQUIRE(path.status == NavPathStatus::Found);
  bool through_gap = false;
  for (const NavGrid::Node node : path.nodes) through_gap = through_gap || grid.position(node).z > 2.0f;
  CHECK(through_gap);
  const std::vector<NavGrid::Node> pulled = pull_straight(grid, path.nodes);
  CHECK(nav_path_length(grid, pulled) < nav_path_length(grid, path.nodes));
}

TEST_CASE("a block lower than a step is climbed; one higher is stood on but not reached") {
  Level level;
  level.add_floor();
  level.add_box({2.5f, 0.15f, -2.5f}, {1, 0.15f, 1});  // 0.3 m: a step
  level.add_box({2.5f, 0.3f, 2.5f}, {1, 0.3f, 1});     // 0.6 m: higher than a step
  const NavGrid grid = level.bake();
  CHECK(on_nav_grid(grid, {2.5f, 0.3f, -2.5f}, kHeadroom));
  CHECK(on_nav_grid(grid, {2.5f, 0.6f, 2.5f}, kHeadroom));
  CHECK(path_between(grid, kWest, {2.5f, 0.3f, -2.5f}).status == NavPathStatus::Found);
  CHECK(path_between(grid, kWest, {2.5f, 0.6f, 2.5f}).status == NavPathStatus::Unreachable);
}

TEST_CASE("a gentle slope is walked up; a slope too steep has no cell on it") {
  constexpr float kGentle = 20.0f;
  constexpr float kSteep = 50.0f;  // the settings allow 45
  Level gentle_level;
  gentle_level.add_floor();
  const Ramp gentle{{2, 0.59f, 0}, kGentle};  // its low end on the floor
  gentle_level.add_box(gentle.center, gentle.half(), tilted(kGentle));
  const NavGrid gentle_grid = gentle_level.bake();
  const Vec3 up_there = gentle.face_at(1.5f);
  REQUIRE(on_nav_grid(gentle_grid, up_there, kHeadroom));
  CHECK(path_between(gentle_grid, kWest, up_there).status == NavPathStatus::Found);

  Level steep_level;
  steep_level.add_floor();
  const Ramp steep{{2, 1.6f, 0}, kSteep};
  steep_level.add_box(steep.center, steep.half(), tilted(kSteep));
  const NavGrid steep_grid = steep_level.bake();
  for (const float along : {-1.0f, -0.5f, 0.0f, 0.5f, 1.0f}) {
    CHECK_FALSE(on_nav_grid(steep_grid, steep.face_at(along), kHeightTolerance));
  }
}

TEST_CASE("a goal across a gap between two floors cannot be reached") {
  Level level;
  level.add_box({-3, -0.5f, 0}, {2, 0.5f, 5});
  level.add_box({3, -0.5f, 0}, {2, 0.5f, 5});
  const NavGrid grid = level.bake();
  CHECK(on_nav_grid(grid, kWest, kHeadroom));
  CHECK(on_nav_grid(grid, kEast, kHeadroom));
  CHECK_FALSE(on_nav_grid(grid, {0, 0, 0}, kHeadroom));
  CHECK(path_between(grid, kWest, kEast).status == NavPathStatus::Unreachable);
}

TEST_CASE("a bridge high enough to walk under gives its columns two cells; a low ceiling none below") {
  Level level;
  level.add_floor();
  level.add_box({-2.5f, 2.5f, 0}, {1, 0.1f, 5});  // its underside 2.4 m up: room for 1.8 m
  level.add_box({2.5f, 1.3f, 0}, {1, 0.1f, 5});   // its underside 1.2 m up: no room
  const NavGrid grid = level.bake();
  const auto bridge = grid.column_at(-2.5f, 0.0f);
  REQUIRE(bridge.has_value());
  const auto [first, last] = grid.nodes_in(*bridge);
  CHECK(last - first == 2);
  CHECK(on_nav_grid(grid, {-2.5f, 2.6f, 0}, kHeadroom));
  CHECK(on_nav_grid(grid, {-2.5f, 0.0f, 0}, kHeadroom));
  CHECK(on_nav_grid(grid, {2.5f, 1.4f, 0}, kHeadroom));
  CHECK_FALSE(on_nav_grid(grid, {2.5f, 0.0f, 0}, kHeadroom));
}

TEST_CASE("moving bodies are not part of the level") {
  Level level;
  level.add_floor();
  level.add_box({0, 0.5f, 0}, {0.5f, 0.5f, 0.5f}, Quat::identity(), "dynamic");
  const NavGrid grid = level.bake();
  CHECK(on_nav_grid(grid, {0.0f, 0.0f, 0.0f}, kHeadroom));
  CHECK_FALSE(any_cell_above(grid, kHeightTolerance));
}

TEST_CASE("a level too large for its cells is baked with larger ones, within the column limit") {
  constexpr float kHuge = 5000.0f;
  Bounds area;
  area.include({-kHuge, 0, -kHuge});
  area.include({kHuge, 0, kHuge});
  const NavGridShape shape = nav_grid_shape(area, Level::settings_for_tests());
  CHECK(shape.cell_size > kCellSize);
  CHECK(shape.column_count() <= nav::kMaxBakeColumns);
  CHECK(nav_grid_shape(Level::floor_area(), Level::settings_for_tests()).cell_size == kCellSize);
  CHECK(nav_grid_shape(Bounds{}, Level::settings_for_tests()).column_count() == 0);
}

TEST_CASE("what baking a 64 m level and searching across it cost") {
  constexpr float kHalfSide = 32.0f;
  constexpr int kWalls = 7;
  constexpr float kWallSpacing = 8.0f;
  constexpr float kGapSide = 4.0f;  // walls are shifted this far, leaving a gap at alternate ends
  Level level;
  level.add_box({0, -0.5f, 0}, {kHalfSide, 0.5f, kHalfSide});
  for (int wall = 0; wall < kWalls; ++wall) {
    const float x = -kHalfSide + kWallSpacing * static_cast<float>(wall + 1);
    const float z = wall % 2 == 0 ? kGapSide : -kGapSide;
    level.add_box({x, 1, z}, {0.25f, 1, kHalfSide - kGapSide});
  }
  Bounds area;
  area.include({-kHalfSide, -1, -kHalfSide});
  area.include({kHalfSide, 2, kHalfSide});

  const auto baking = std::chrono::steady_clock::now();
  const NavGrid grid = level.bake(area);
  const std::chrono::duration<double, std::milli> bake_time = std::chrono::steady_clock::now() - baking;
  CHECK(grid.shape().column_count() == 256 * 256);
  // (Printed, for the cost to be read in any run's log.)
  std::cout << "navigation cost: bake of " << grid.shape().column_count() << " columns (" << grid.node_count()
            << " walkable cells) in " << bake_time.count() << " ms\n";

  const Vec3 corners[] = {{30, 0, -30}, {-30, 0, -30}};  // along a corridor; through the maze
  for (const Vec3& goal : corners) {
    const auto searching = std::chrono::steady_clock::now();
    const NavGridPath path = path_between(grid, {30, 0, 30}, goal);
    const std::chrono::duration<double, std::milli> search_time = std::chrono::steady_clock::now() - searching;
    CHECK(path.searched <= nav::kMaxSearchNodes);
    std::cout << "navigation cost: search " << (path.status == NavPathStatus::Found ? "found a path" : "found none")
              << " after looking at " << path.searched << " cells, in " << search_time.count() << " ms\n";
  }
}
