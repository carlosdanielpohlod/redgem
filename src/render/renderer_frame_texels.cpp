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

constexpr int kTexelFormat = RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32A32;
constexpr int kOneLevel = 1;  // no smaller copies: texels are fetched one by one, never filtered

}  // namespace

void Renderer::Impl::write_frame_texels(const Scene& scene) {
  const std::size_t count = std::min(scene.size(), worlds.size());
  sights.assign(count, ObjectSight{});
  std::size_t next_bone = render_math::record_texel(count);  // the bones come after the records
  for (std::size_t slot = 0; slot < count; ++slot) {
    const GameObject& object = scene.at(static_cast<ObjectId>(slot));
    if (!render_math::has_something_to_draw(object, show_prefabs, meshes.size())) continue;
    next_bone = sight_object(scene, object, next_bone);
  }
}

std::size_t Renderer::Impl::sight_object(const Scene& scene, const GameObject& object, std::size_t next_bone) {
  const auto slot = static_cast<std::size_t>(object.id);
  const Bounds& bounds = mesh_bounds[static_cast<std::size_t>(object.mesh)];
  const std::optional<SkeletonPose> pose = pose_skeleton(scene, object, next_bone);
  const render_math::InstanceLook look{object.tint, pose ? static_cast<float>(next_bone) : render_math::kNoBones};
  ObjectSight& sight = sights[slot];
  sight.drawn = render_math::write_instance(texels, slot, model_matrix(worlds[slot]), look);
  if (!pose) {
    sight.sight = render_math::sight_of(bounds, worlds[slot]);
    return next_bone;
  }
  sight.sight = render_math::skinned_sight_of(bounds, worlds[slot], pose->first);
  return next_bone + pose->bones * kTexelsPerMatrix;
}

std::optional<SkeletonPose> Renderer::Impl::pose_skeleton(const Scene& scene, const GameObject& object,
                                                          std::size_t first) {
  if (!is_index_of(object.skin, skins.size())) return std::nullopt;
  const SkinDesc& skin = skins[static_cast<std::size_t>(object.skin)];
  const std::size_t bones = std::min(object.joints.size(), skin.inverse_bind.size());
  std::optional<Matrix> first_bone;
  for (std::size_t j = 0; j < bones; ++j) {
    const std::optional<Matrix> bone =
        write_bone(scene, object.joints[j], skin.inverse_bind[j], first + j * kTexelsPerMatrix);
    if (!bone) return std::nullopt;
    if (j == 0) first_bone = bone;
  }
  if (!first_bone) return std::nullopt;  // a skeleton without bones
  return SkeletonPose{*first_bone, bones};
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
  if (texels.grew()) {
    remake_texel_texture();
  } else {
    upload_changed_rows();
  }
  bind_for_the_frame(shader, lit_uniforms.frame_texels, kFrameTexelSlot, texel_texture);
  bind_for_the_frame(depth_shader, depth_uniforms.frame_texels, kFrameTexelSlot, texel_texture);
}

void Renderer::Impl::remake_texel_texture() {
  if (texel_texture != 0) rlUnloadTexture(texel_texture);
  texel_texture = rlLoadTexture(texels.row(0), kTexelsPerRow, texels.rows(), kTexelFormat, kOneLevel);
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
