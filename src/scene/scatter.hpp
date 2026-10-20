#pragma once

// Copies of a mesh placed in great numbers that are not objects of their own: a meadow's grass, a
// forest's trees, the rubble of a city (Scatter in Ruby). They are only drawn: nothing simulates,
// collides with or scripts them, so a scene can have tens of thousands. Where they go is worked
// out here, and is the same for the same seed on every machine.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <vector>

#include "core/math.hpp"
#include "scene/scene_desc.hpp"
#include "scene/transform.hpp"

namespace redgem {

// The most drawing records a scene's sets may hold together, counting every LOD.
inline constexpr std::size_t kMaxScatteredCopies = 262144;

// A lighter mesh taking over from this distance, in meters. It shares the base template's
// transform and tint; models must use the same origin, size and orientation.
struct ScatterLod {
  int mesh = kNoIndex;
  float from = 0.0f;
};

// A representation's drawing range, including overlap for dithered transitions.
struct ScatterLevel {
  int mesh = kNoIndex;
  float from = 0.0f;
  float to = std::numeric_limits<float>::infinity();
};

// Copies of one mesh, as its object looked when they were placed.
struct ScatterSet {
  int id = 0;
  std::vector<ScatterLod> lods;  // ordered by distance, after the base mesh
  int mesh = kNoIndex;
  Vec3 tint = Vec3::one();
  // Drawn only while this far from the camera, in meters (see GameObject).
  float visible_from = 0.0f;
  float visible_to = std::numeric_limits<float>::infinity();
  bool cast_shadow = true;
  bool sways = true;  // whether wind sways its foliage (see GameObject)
  std::vector<Transform> places;
  std::vector<Vec3> tints;  // one per place, multiplying `tint`; empty: all alike
  // Meters over which copies fade out before `visible_to` (and in after a `visible_from` above
  // zero) instead of popping; 0: they do not fade.
  float fade = 0.0f;
  float normals_up = 0.0f;  // 0..1: how far the copies' normals are bent toward their own up (grass lit as a meadow)
};

// Drawing ranges for the base mesh and its lighter replacements. Adjacent levels overlap
// for `fade` meters, keeping complementary pixels through the existing dither.
std::vector<ScatterLevel> scatter_levels(const ScatterSet& set);

// What copies are asked for: `count` of them over a box (`low` its corner with the smallest x, y
// and z, `high` the opposite one), each turned at random around the vertical and scaled between
// `smallest` and `largest`.
struct ScatterRequest {
  std::size_t count = 0;
  Vec3 low;
  Vec3 high;
  std::uint32_t seed = 0;
  float smallest = 1.0f;
  float largest = 1.0f;
  bool align = false;  // stands along the ground's slope instead of upright
};

// Where a ray straight down from `above` meets what copies stand on, within `depth` meters.
struct GroundHit {
  Vec3 point;
  Vec3 normal = Vec3::up();
};
using GroundProbe = std::function<std::optional<GroundHit>(Vec3 above, float depth)>;

// Where the copies go. With a ground, each stands where a ray from the top of the box down meets
// it, and is not placed where it does not; without one (an empty probe), on the bottom of the box.
// Every try takes its random numbers whether or not it is placed: the others stay where they were.
std::vector<Transform> scatter_places(const ScatterRequest& request, const GroundProbe& ground);

// Every place scaled by the template's own scale, on top of its own.
void scale_places(std::vector<Transform>& places, Vec3 scale);

// Every set of copies of a scene.
class Scattered {
 public:
  // Adds a set and returns its id, by which it is removed. Nothing (0) when there would be more
  // than kMaxScatteredCopies drawing records (including LODs).
  int add(ScatterSet set);
  bool remove(int id);
  void clear();
  std::size_t copies() const;
  // Drawing records, counting each copy once for every representation.
  std::size_t records() const;
  const std::vector<ScatterSet>& sets() const { return sets_; }
  // Changes whenever the sets do, and is never the same for two scenes of a session (so whoever
  // drew another scene's copies knows to start over).
  std::uint64_t version() const { return version_; }

 private:
  void changed();

  std::vector<ScatterSet> sets_;
  int next_id_ = 1;
  std::uint64_t version_ = 0;
};

}  // namespace redgem
