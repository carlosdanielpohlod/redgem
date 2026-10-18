// What the vertex shader reads of a frame besides its vertices (texel_rows.hpp): the record of every
// object (instance_records.hpp: where it is, its tint, where its bones are) and the bones of every
// skeleton, and how each pass judges every object, worked out once a frame for all its passes.

#include "render/renderer_impl.hpp"

#include <raymath.h>

#include <algorithm>

#include "render/instance_records.hpp"

namespace redgem {
namespace {

using namespace render_detail;
using render_math::kTexelsPerMatrix;
using render_math::kTexelsPerRow;
using render_math::model_matrix;

using render_math::kTexelFormat;
using render_math::kTexelLevels;
bool same(Vec3 a, Vec3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
bool same(Quat a, Quat b) { return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w; }
bool same(const Transform& a, const Transform& b) {
  return same(a.position, b.position) && same(a.rotation, b.rotation) && same(a.scale, b.scale);
}

// Whether a record would be written from the very same as last time.
bool same(const RecordSource& a, const RecordSource& b) {
  return same(a.world, b.world) && same(a.tint, b.tint) && a.first_bone == b.first_bone && a.sway == b.sway &&
         a.record == b.record;
}

}  // namespace

void Renderer::Impl::write_frame_texels(const Scene& scene) {
  lay_out_scattered(scene.scattered());
  const std::size_t count = std::min(scene.size(), worlds.size());
  sights.assign(count, ObjectSight{});
  record_sources.resize(count);  // (a slot past the end that comes back is written anew)
  record_owners.resize(count);
  std::size_t next_bone = render_math::record_texel(scattered_records + count);  // after every record
  for (std::size_t slot = 0; slot < count; ++slot) {
    const GameObject& object = scene.at(static_cast<ObjectId>(slot));
    if (!render_math::has_something_to_draw(object, show_prefabs, meshes.size())) {
      record_owners[slot] = kNoHandle;
      continue;
    }
    next_bone = sight_object(scene, object, next_bone);
  }
}

std::size_t Renderer::Impl::sight_object(const Scene& scene, const GameObject& object, std::size_t next_bone) {
  const auto slot = static_cast<std::size_t>(object.id);
  const Bounds& bounds = mesh_bounds[static_cast<std::size_t>(object.mesh)];
  const std::optional<SkeletonPose> pose = pose_skeleton(scene, object, next_bone);
  render_math::InstanceLook look{object.tint, pose ? static_cast<float>(next_bone) : render_math::kNoBones};
  look.sway = object.sways ? render_math::kSways : render_math::kStill;
  if (temporal.on) note_fresh_record(scene, object, look.first_bone, pose ? pose->bones : 0);
  const render_math::Sight seen = pose ? render_math::skinned_sight_of(bounds, worlds[slot], pose->first)
                                       : render_math::sight_of(bounds, worlds[slot]);
  sights[slot] = ObjectSight{write_record(slot, look), seen, object.mesh, object.visible_from, object.visible_to,
                             object.cast_shadow};
  return pose ? next_bone + pose->bones * kTexelsPerMatrix : next_bone;
}

bool Renderer::Impl::write_record(std::size_t slot, const render_math::InstanceLook& look) {
  const std::size_t record = scattered_records + slot;
  const RecordSource source{worlds[slot], look.tint, look.first_bone, look.sway, static_cast<float>(record)};
  RecordSource& last = record_sources[slot];
  if (same(source, last)) return true;
  if (!render_math::write_instance(texels, record, model_matrix(worlds[slot]), look)) return false;
  last = source;
  return true;
}

std::optional<SkeletonPose> Renderer::Impl::pose_skeleton(const Scene& scene, const GameObject& object,
                                                          std::size_t first) {
  if (!is_index_of(object.skin, skins.size())) return std::nullopt;
  const SkinDesc& skin = skins[static_cast<std::size_t>(object.skin)];
  const std::size_t bones = std::min(object.joints.size(), skin.inverse_bind.size());
  const std::optional<Matrix> first_bone = write_bones(scene, object, skin, first, bones);
  if (!first_bone) return std::nullopt;
  return SkeletonPose{*first_bone, bones};
}

std::optional<Matrix> Renderer::Impl::write_bones(const Scene& scene, const GameObject& object, const SkinDesc& skin,
                                                  std::size_t first, std::size_t bones) {
  std::optional<Matrix> first_bone;  // (none for a skeleton without bones)
  for (std::size_t j = 0; j < bones; ++j) {
    const std::optional<Matrix> bone =
        write_bone(scene, object.joints[j], skin.inverse_bind[j], first + j * kTexelsPerMatrix);
    if (!bone) return std::nullopt;
    if (j == 0) first_bone = bone;
  }
  return first_bone;
}

std::optional<Matrix> Renderer::Impl::write_bone(const Scene& scene, ObjectHandle joint,
                                                 const std::array<float, kFloatsPerMatrix>& inverse_bind,
                                                 std::size_t texel) {
  const ObjectId bone = scene.resolve(joint);
  if (bone == kNoObject) return std::nullopt;
  const Matrix matrix = MatrixMultiply(render_math::from_column_major(inverse_bind),
                                       model_matrix(worlds[static_cast<std::size_t>(bone)]));
  if (!texels.write_matrix(texel, matrix)) return std::nullopt;
  return matrix;
}

// --- To the graphics card ---------------------------------------------------------------------

void Renderer::Impl::send_frame_texels() {
  if (texels.rows() == 0) return;  // nothing was ever written: nothing reads it either
  const bool kept_before = temporal.on && !texels.grew();
  if (kept_before) keep_previous_texels();
  if (texels.grew()) {
    remake_texel_texture();
  } else {
    upload_changed_rows();
  }
  if (temporal.on && !kept_before) keep_previous_texels();
  if (temporal.on) carry_fresh_texels();
  bind_for_the_frame(shader, lit_uniforms.frame_texels, kFrameTexelSlot, texel_texture);
  bind_for_the_frame(depth_shader, depth_uniforms.frame_texels, kFrameTexelSlot, texel_texture);
}

void Renderer::Impl::remake_texel_texture() {
  if (texel_texture != 0) rlUnloadTexture(texel_texture);
  texel_texture = rlLoadTexture(texels.row(0), kTexelsPerRow, texels.rows(), kTexelFormat, kTexelLevels);
  texel_texture_rows = texels.rows();
  texels.taken_whole();
  texels.take_changed(changed_rows);  // all of them went
}

void Renderer::Impl::upload_changed_rows() {
  texels.take_changed(changed_rows);
  for (const render_math::RowRun& run : changed_rows) {
    rlUpdateTexture(texel_texture, 0, run.first, kTexelsPerRow, run.count, kTexelFormat, texels.row(run.first));
  }
}

}  // namespace redgem
