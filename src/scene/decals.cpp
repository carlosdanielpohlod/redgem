#include "scene/decals.hpp"

#include <algorithm>
#include <utility>

namespace redgem {
namespace {

// Meters a decal's corners are raised off the surface they lie on, so that they are not drawn
// into it where its depth and theirs come out a hair apart.
constexpr float kLift = 0.002f;
constexpr std::size_t kCornerFloats = 3;
constexpr std::size_t kTriangleIndices = 3;

Vec3 to_world(const Transform& world, Vec3 local) {
  return world.position + rotate(world.rotation, hadamard(world.scale, local));
}

Vec3 to_local(const Transform& world, Vec3 point) {
  return safe_divide(rotate(conjugate(world.rotation), point - world.position), world.scale);
}

bool overlaps(const Bounds& a, const Bounds& b) {
  return a.min.x <= b.max.x && b.min.x <= a.max.x && a.min.y <= b.max.y && b.min.y <= a.max.y &&
         a.min.z <= b.max.z && b.min.z <= a.max.z;
}

// What the box reaches of an object's own space, as the smallest box along its axes around it.
Bounds local_reach(const DecalBox& box, const Transform& world) {
  Bounds reach;
  for (const Vec3 corner : box_corners(box)) reach.include(to_local(world, corner));
  return reach;
}

std::array<Vec3, 3> local_triangle(const PrimitiveDesc& primitive, std::size_t first_index) {
  std::array<Vec3, 3> triangle{};
  for (std::size_t corner = 0; corner < kTriangleIndices; ++corner) {
    triangle[corner] = vec3_from(&primitive.positions[primitive.indices[first_index + corner] * kCornerFloats]);
  }
  return triangle;
}

bool within_reach(const std::array<Vec3, 3>& triangle, const Bounds& reach) {
  Bounds around;
  for (const Vec3 corner : triangle) around.include(corner);
  return overlaps(around, reach);
}

// The triangle in the world, its corners in the order that faces out of its surface (a mirroring
// placement turns them the other way).
std::array<Vec3, 3> world_triangle(const std::array<Vec3, 3>& local, const Transform& world) {
  std::array<Vec3, 3> triangle{to_world(world, local[0]), to_world(world, local[1]), to_world(world, local[2])};
  if (mirrors(world)) std::swap(triangle[1], triangle[2]);
  return triangle;
}

// One object's surfaces under the box, and how many more corners the decal may have.
struct Cut {
  const DecalBox& box;
  const Transform& world;
  Bounds reach;
  std::size_t room = 0;
};

void cut_primitive(const Cut& cut, const PrimitiveDesc& primitive, std::vector<DecalCorner>& out) {
  for (std::size_t first = 0; first + kTriangleIndices <= primitive.indices.size(); first += kTriangleIndices) {
    if (out.size() >= cut.room) return;
    const std::array<Vec3, 3> local = local_triangle(primitive, first);
    if (within_reach(local, cut.reach)) clip_to_decal(cut.box, world_triangle(local, cut.world), out);
  }
}

bool is_water(const SceneDesc& desc, const PrimitiveDesc& primitive) {
  const bool has_material = primitive.material >= 0 && static_cast<std::size_t>(primitive.material) < desc.materials.size();
  return has_material && desc.materials[static_cast<std::size_t>(primitive.material)].water.enabled;
}

std::vector<DecalCorner> cut_mesh(const Cut& cut, const MeshDesc& mesh, const SceneDesc& desc) {
  std::vector<DecalCorner> corners;
  if (mesh.bounds.valid && !overlaps(mesh.bounds, cut.reach)) return corners;
  for (const PrimitiveDesc& primitive : mesh.primitives) {
    if (!is_water(desc, primitive)) cut_primitive(cut, primitive, corners);
  }
  corners.resize(std::min(corners.size(), cut.room));
  return corners;
}

// The piece of a decal on one object: its corners raised off the surface, in the object's space.
DecalPiece piece_on(const Scene& scene, ObjectId id, const SceneDesc& desc, const DecalBox& box, std::size_t room) {
  const Transform world = scene.world_transform(id);
  const Cut cut{box, world, local_reach(box, world), room};
  std::vector<DecalCorner> corners = cut_mesh(cut, desc.meshes[static_cast<std::size_t>(scene.at(id).mesh)], desc);
  for (DecalCorner& corner : corners) corner.position = to_local(world, corner.position + box.normal * kLift);
  return {scene.handle(id), std::move(corners)};
}

bool rigid_mesh(const GameObject& object, const SceneDesc& desc) {
  const bool has_mesh = object.mesh >= 0 && static_cast<std::size_t>(object.mesh) < desc.meshes.size();
  return has_mesh && object.skin == kNoIndex && desc.meshes[static_cast<std::size_t>(object.mesh)].target_names.empty();
}

// Whether an object is part of the view model (it or one above it is): a weapon held in front of
// the camera is drawn apart from the world, so the world's marks do not fall on it.
bool held_as_view_model(const Scene& scene, ObjectId id) {
  for (ObjectId at = id; at != kNoObject; at = scene.at(at).parent) {
    if (scene.at(at).view_model) return true;
  }
  return false;
}

// Whether the world's marks fall on an object: one that takes decals, not held as the view model.
bool marked_in_the_world(const Scene& scene, ObjectId id, const SceneDesc& desc) {
  return takes_decals(scene.at(id), desc) && !held_as_view_model(scene, id);
}

std::size_t corners_of(const Decal& decal) {
  std::size_t corners = 0;
  for (const DecalPiece& piece : decal.pieces) corners += piece.corners.size();
  return corners;
}

}  // namespace

bool takes_decals(const GameObject& object, const SceneDesc& desc) {
  return object.visible && !object.prefab && !object.view_model && rigid_mesh(object, desc);
}

bool DecalSystem::place(const Scene& scene, const SceneDesc& desc, const DecalPlacement& placement) {
  const DecalBox box =
      decal_box(placement.point, placement.normal, placement.size, placement.depth, placement.spin_radians);
  Decal decal{{}, placement.image, placement.color};
  for (const ObjectId id : scene.alive_ids()) {
    if (!marked_in_the_world(scene, id, desc)) continue;
    DecalPiece piece = piece_on(scene, id, desc, box, kMaxDecalCorners - corners_of(decal));
    if (!piece.corners.empty()) decal.pieces.push_back(std::move(piece));
  }
  if (decal.pieces.empty()) return false;
  decals_.push_back(std::move(decal));
  set_limit(limit_);
  return true;
}

void DecalSystem::set_limit(std::size_t limit) {
  limit_ = std::clamp(limit, kMinDecalLimit, kMaxDecalLimit);
  while (decals_.size() > limit_) decals_.pop_front();
}

std::vector<DecalCorner> world_corners(const DecalPiece& piece, const Transform& host_world) {
  std::vector<DecalCorner> corners = piece.corners;
  for (DecalCorner& corner : corners) corner.position = to_world(host_world, corner.position);
  return corners;
}

}  // namespace redgem
