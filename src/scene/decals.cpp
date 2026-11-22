#include "scene/decals.hpp"

#include <algorithm>
#include <utility>

namespace redgem {
namespace {

// Meters a decal's corners are raised off the surface they lie on, so that they are not drawn
// into it where its depth and theirs come out a hair apart.
constexpr float kLift = 0.002f;

Vec3 to_world(const Transform& world, Vec3 local) {
  return world.position + rotate(world.rotation, hadamard(world.scale, local));
}

Vec3 to_local(const Transform& world, Vec3 point) {
  return safe_divide(rotate(conjugate(world.rotation), point - world.position), world.scale);
}

// What the box reaches of an object's own space, as the smallest box along its axes around it.
Bounds local_reach(const DecalBox& box, const Transform& world) {
  Bounds reach;
  for (const Vec3 corner : box_corners(box)) reach.include(to_local(world, corner));
  return reach;
}

DecalTriangle local_triangle(const PrimitiveDesc& primitive, std::size_t first_index) {
  DecalTriangle triangle{};
  for (std::size_t corner = 0; corner < kTriangleCorners; ++corner) {
    triangle[corner] = listed_corner(primitive, first_index + corner);
  }
  return triangle;
}

bool within_reach(const DecalTriangle& triangle, const Bounds& reach) {
  Bounds around;
  for (const Vec3 corner : triangle) around.include(corner);
  return boxes_overlap(around, reach);
}

// The triangle in the world, its corners in the order that faces out of its surface (a mirroring
// placement turns them the other way).
DecalTriangle world_triangle(const DecalTriangle& local, const Transform& world) {
  DecalTriangle triangle{};
  for (std::size_t corner = 0; corner < kTriangleCorners; ++corner) triangle[corner] = to_world(world, local[corner]);
  if (mirrors(world)) std::swap(triangle[kSecondCorner], triangle[kThirdCorner]);
  return triangle;
}

// One object's surfaces under the box, how many more corners the decal may have, and where the
// triangles of dense surfaces are sorted to be found.
struct Cut {
  const DecalBox& box;
  const Transform& world;
  Bounds reach;
  std::size_t room = 0;
  TriangleIndexCache& indexes;
};

// Raises the corners a triangle gave the decal (from `first` on) off it, on the side the decal is.
void lift_off(const DecalTriangle& triangle, Vec3 toward, std::vector<DecalCorner>& out, std::size_t first) {
  const Vec3 facing = normalize(triangle_facing(triangle));
  const Vec3 out_of = dot(facing, toward) < 0.0f ? -facing : facing;
  for (std::size_t corner = first; corner < out.size(); ++corner) {
    out[corner].position = out[corner].position + out_of * kLift;
  }
}

void cut_triangle(const Cut& cut, const DecalTriangle& triangle, bool either_side, std::vector<DecalCorner>& out) {
  const std::size_t first = out.size();
  clip_to_decal(cut.box, triangle, out, either_side);
  lift_off(triangle, cut.box.normal, out, first);
}

// The triangle whose corners are listed from `first` on, if it is near the box.
void cut_listed(const Cut& cut, const PrimitiveDesc& primitive, std::size_t first, bool either_side,
                std::vector<DecalCorner>& out) {
  const DecalTriangle local = local_triangle(primitive, first);
  if (within_reach(local, cut.reach)) cut_triangle(cut, world_triangle(local, cut.world), either_side, out);
}

// The triangles of a dense primitive near the box, in the order the primitive lists them.
std::vector<std::size_t> near_the_box(const TriangleIndex& index, const Bounds& reach) {
  std::vector<std::size_t> near;
  index.find(reach, near);
  std::sort(near.begin(), near.end());
  return near;
}

// Where each of a small primitive's triangles is listed from.
std::vector<std::size_t> every_triangle(const PrimitiveDesc& primitive) {
  std::vector<std::size_t> firsts;
  for (std::size_t first = 0; first + kTriangleCorners <= listed_corners(primitive); first += kTriangleCorners) {
    firsts.push_back(first);
  }
  return firsts;
}

// A two-sided surface takes a mark on either side. A dense one is looked up in its index; a small
// one walked whole.
void cut_primitive(const Cut& cut, const PrimitiveDesc& primitive, bool either_side, std::vector<DecalCorner>& out) {
  const TriangleIndex* index = cut.indexes.of(primitive);
  for (const std::size_t first : index != nullptr ? near_the_box(*index, cut.reach) : every_triangle(primitive)) {
    if (out.size() >= cut.room) return;
    cut_listed(cut, primitive, first, either_side, out);
  }
}

const MaterialDesc* material_of(const SceneDesc& desc, const PrimitiveDesc& primitive) {
  const bool known = primitive.material >= 0 && static_cast<std::size_t>(primitive.material) < desc.materials.size();
  return known ? &desc.materials[static_cast<std::size_t>(primitive.material)] : nullptr;
}

// Not water, nor see-through or cut out: decals are drawn before what is see-through, and a mark on
// it, or on a cut-out's holes, would darken what is behind.
bool solid(const MaterialDesc& material) { return material.alpha_mode == AlphaMode::Opaque && !material.water.enabled; }

// A primitive marks fall on: of no material, or a solid one.
bool takes_marks(const SceneDesc& desc, const PrimitiveDesc& primitive) {
  const MaterialDesc* material = material_of(desc, primitive);
  return material == nullptr || solid(*material);
}

bool two_sided(const SceneDesc& desc, const PrimitiveDesc& primitive) {
  const MaterialDesc* material = material_of(desc, primitive);
  return material != nullptr && material->double_sided;
}

std::vector<DecalCorner> cut_mesh(const Cut& cut, const MeshDesc& mesh, const SceneDesc& desc) {
  std::vector<DecalCorner> corners;
  if (mesh.bounds.valid && !boxes_overlap(mesh.bounds, cut.reach)) return corners;
  for (const PrimitiveDesc& primitive : mesh.primitives) {
    if (takes_marks(desc, primitive)) cut_primitive(cut, primitive, two_sided(desc, primitive), corners);
  }
  corners.resize(std::min(corners.size(), cut.room));
  return corners;
}

// The piece of a decal on one object: its corners raised off the surface, in the object's space.
DecalPiece piece_on(const Scene& scene, ObjectId id, const SceneDesc& desc, const DecalBox& box, std::size_t room,
                    TriangleIndexCache& indexes) {
  const Transform world = scene.world_transform(id);
  const Cut cut{box, world, local_reach(box, world), room, indexes};
  std::vector<DecalCorner> corners = cut_mesh(cut, desc.meshes[static_cast<std::size_t>(scene.at(id).mesh)], desc);
  for (DecalCorner& corner : corners) corner.position = to_local(world, corner.position);
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

std::size_t mesh_of(const Scene& scene, ObjectId id) { return static_cast<std::size_t>(scene.at(id).mesh); }

// Sorts the triangles of the parts of a mesh marks fall on (those dense enough to be).
void sort_mesh(TriangleIndexCache& indexes, const SceneDesc& desc, const MeshDesc& mesh) {
  for (const PrimitiveDesc& primitive : mesh.primitives) {
    if (takes_marks(desc, primitive)) indexes.of(primitive);
  }
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
    DecalPiece piece = piece_on(scene, id, desc, box, kMaxDecalCorners - corners_of(decal), indexes_);
    if (!piece.corners.empty()) decal.pieces.push_back(std::move(piece));
  }
  if (decal.pieces.empty()) return false;
  decals_.push_back(std::move(decal));
  set_limit(limit_);
  return true;
}

void DecalSystem::start_over(const Scene* scene, const SceneDesc& desc) {
  decals_.clear();
  indexes_.forget();
  if (scene == nullptr) return;
  for (const ObjectId id : scene->alive_ids()) {
    if (marked_in_the_world(*scene, id, desc)) sort_mesh(indexes_, desc, desc.meshes[mesh_of(*scene, id)]);
  }
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
