#pragma once

// Navigation: where the characters a game moves can walk, and the way from one place to another
// (the Navigation module of the scripts). A scene's static bodies are baked into a grid
// (navigation/nav_bake.hpp) as it loads, or when a script first asks; paths are found on it
// (navigation/nav_path.hpp), and characters steer clear of each other and of moving bodies on
// their way (navigation/nav_steering.hpp).
//
// What it does not do yet: bodies that move (doors, crates, other characters) are not in the grid
// (a path goes around those it is told of, and steering around the rest), a level that changes
// after the bake is not seen until Navigation.bake, and there are no off-grid links (jumps,
// ladders, teleporters).

#include <optional>
#include <vector>

#include "core/math.hpp"
#include "navigation/nav_bake.hpp"
#include "navigation/nav_grid.hpp"
#include "navigation/nav_path.hpp"
#include "navigation/nav_settings.hpp"
#include "navigation/nav_steering.hpp"
#include "physics/physics_world.hpp"

namespace redgem {

class Navigation {
 public:
  void configure(const NavSettings& settings) { settings_ = settings; }
  const NavSettings& settings() const { return settings_; }

  // Forgets the grid (a new scene): the next question bakes one, unless one is baked first.
  void forget() { grid_.reset(); }
  // Bakes the grid of the level in a physics world now (its static bodies), replacing any.
  const NavBakeReport& bake(const PhysicsWorld& physics);
  bool baked() const { return grid_.has_value(); }
  const NavBakeReport& report() const { return report_; }
  // The baked grid (an empty one before a bake).
  const NavGrid& grid() const;

  // The way from near `from` to near `to` (their nearest walkable points): points to walk through,
  // pulled straight, the first and the last included. None when there is no way, or the search
  // ran out of its budget. It keeps a character's width off the obstacles given (bodies that move).
  std::optional<std::vector<Vec3>> path(Vec3 from, Vec3 to, const std::vector<NavObstacle>& around = {}) const;
  // The velocity a character walks at among others (see nav_steering.hpp), kept to walkable ground:
  // none takes it off the grid within nav::kSteeringLookahead seconds, unless it is off already.
  Vec3 steer(const SteeringAgent& agent, const std::vector<SteeringObstacle>& others) const;
  // Whether walking from a point by an offset stays on walkable ground, checked a cell at a time
  // (at most nav::kMostSteeringChecks times): no gap or wall is stepped over.
  bool walks_on(Vec3 from, Vec3 offset) const;
  std::optional<Vec3> nearest(Vec3 point) const;
  // Whether a point is on walkable ground: over a walkable cell, up to a character's height above it.
  bool walkable(Vec3 point) const;

 private:
  NavSettings settings_;
  std::optional<NavGrid> grid_;
  NavBakeReport report_;
};

}  // namespace redgem
