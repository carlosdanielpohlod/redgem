#pragma once

// Navigation: where the characters a game moves can walk, and the way from one place to another
// (the Navigation module of the scripts). A scene's static bodies are baked into a grid
// (navigation/nav_bake.hpp) as it loads, or when a script first asks; paths are found on it
// (navigation/nav_path.hpp).
//
// What it does not do yet: bodies that move (doors, crates, other characters) are not obstacles,
// a level that changes after the bake is not seen until Navigation.bake, and there are no
// off-grid links (jumps, ladders, teleporters).

#include <optional>
#include <vector>

#include "core/math.hpp"
#include "navigation/nav_bake.hpp"
#include "navigation/nav_grid.hpp"
#include "navigation/nav_settings.hpp"
#include "physics/physics_world.hpp"
#include "scene/scene.hpp"
#include "scene/scene_desc.hpp"

namespace redgem {

// What a level is baked from: its objects and the physics world their bodies are in.
struct NavLevel {
  const Scene& scene;
  const SceneDesc& desc;
  const PhysicsWorld& physics;
};

class Navigation {
 public:
  void configure(const NavSettings& settings) { settings_ = settings; }
  const NavSettings& settings() const { return settings_; }

  // Forgets the grid (a new scene): the next question bakes one, unless one is baked first.
  void forget() { grid_.reset(); }
  // Bakes the level's grid now, replacing any.
  const NavBakeReport& bake(const NavLevel& level);
  bool baked() const { return grid_.has_value(); }
  const NavBakeReport& report() const { return report_; }
  // The baked grid (an empty one before a bake).
  const NavGrid& grid() const;

  // The way from near `from` to near `to` (their nearest walkable points): points to walk through,
  // pulled straight, the first and the last included. None when there is no way, or the search
  // ran out of its budget.
  std::optional<std::vector<Vec3>> path(Vec3 from, Vec3 to) const;
  std::optional<Vec3> nearest(Vec3 point) const;
  // Whether a point is on walkable ground: over a walkable cell, up to a character's height above it.
  bool walkable(Vec3 point) const;

 private:
  NavSettings settings_;
  std::optional<NavGrid> grid_;
  NavBakeReport report_;
};

}  // namespace redgem
