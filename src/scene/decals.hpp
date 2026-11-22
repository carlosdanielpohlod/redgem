#pragma once

// Decals: marks laid on the surfaces of the scene (bullet holes, scorches, cracks, blood), each a
// picture projected through a box onto what is there. They are looks only, and local: nothing
// collides with them, and the machine that places one is the only one that sees it.
//
// A placed decal keeps the pieces of the surfaces it fell on, each in its object's own space, so a
// door that swings carries its bullet holes with it; a destroyed or hidden object takes its pieces
// along. Decals are kept up to a limit: past it, placing one drops the oldest.

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "scene/decal_clip.hpp"
#include "scene/scene.hpp"
#include "scene/scene_desc.hpp"

namespace redgem {

inline constexpr std::size_t kDefaultDecalLimit = 256;
inline constexpr std::size_t kMinDecalLimit = 1;
inline constexpr std::size_t kMaxDecalLimit = 4096;
// The most corners one decal is made of, over all the surfaces it falls on (a mark across a dense
// mesh is cut short there rather than costing without bound).
inline constexpr std::size_t kMaxDecalCorners = 3 * 512;

// What a decal is placed with (Decals.place).
struct DecalPlacement {
  Vec3 point;
  Vec3 normal;         // out of the surface
  float size = 0.0f;   // meters across
  float depth = 0.0f;  // meters it reaches into and out of the surface, all told
  float spin_radians = 0.0f;
  std::string image;  // a picture of the game's images/ folder; empty for a soft round spot
  Color color{0.0f, 0.0f, 0.0f, 1.0f};
};

// The part of a decal on one object: its corners in that object's own space, three a triangle.
struct DecalPiece {
  ObjectHandle host;
  std::vector<DecalCorner> corners;
};

struct Decal {
  std::vector<DecalPiece> pieces;
  std::string image;
  Color color;
};

class DecalSystem {
 public:
  // Places a decal on whatever surfaces of `scene` (meshes of `desc`) are inside its box. Returns
  // whether it fell on any: nothing is kept when it did not.
  bool place(const Scene& scene, const SceneDesc& desc, const DecalPlacement& placement);
  void clear() { decals_.clear(); }
  // How many are kept at most (kMinDecalLimit to kMaxDecalLimit); a lower limit drops the oldest
  // at once.
  void set_limit(std::size_t limit);
  std::size_t limit() const { return limit_; }
  // The decals kept, oldest first.
  const std::deque<Decal>& decals() const { return decals_; }

 private:
  std::deque<Decal> decals_;
  std::size_t limit_ = kDefaultDecalLimit;
};

// Whether an object of the scene takes decals: one that is drawn, of a rigid mesh (a skeleton or
// shape keys would move its surface away from the marks).
bool takes_decals(const GameObject& object, const SceneDesc& desc);

// The corners of a piece in the world, its object where it is now.
std::vector<DecalCorner> world_corners(const DecalPiece& piece, const Transform& host_world);

}  // namespace redgem
