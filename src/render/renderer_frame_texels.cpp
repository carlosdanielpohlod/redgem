// What the vertex shader reads of a frame besides its vertices (texel_rows.hpp): the record of every
// object (instance_records.hpp: where it is, its tint, where its bones are) and the bones of every
// skeleton, and how each pass judges every object, worked out once a frame for all its passes.

#include "render/renderer_impl.hpp"

#include <raymath.h>

#include <algorithm>

#include "render/instance_records.hpp"
#include "render/screen_levels.hpp"
#include "scene/transform.hpp"

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

bool same(const render_math::DrawnRange& a, const render_math::DrawnRange& b) {
  return a.from == b.from && a.to == b.to && a.fade == b.fade;
}

// Whether a record would be written from the very same as last time.
bool same(const RecordSource& a, const RecordSource& b) {
  return same(a.world, b.world) && same(a.tint, b.tint) && a.first_bone == b.first_bone &&
         a.first_morph == b.first_morph && a.sway == b.sway && a.record == b.record && same(a.range, b.range);
}

// Where a fading object's record says it fades (none for the rest, whose shader then works out no noise).
void fade_in_look(const render_math::DrawnRange& range, render_math::InstanceLook& look) {
  if (range.fade <= 0.0f) return;
  look.visible_from = range.from;
  look.visible_to = range.to;
  look.fade = range.fade;
}

// The distances the frame judges a fading object by: the shader fades it by where its origin is,
// the frame by its sphere's center, so these take in how far apart those two are.
render_math::DrawnRange judged_range(render_math::DrawnRange range, const render_math::Sight& seen, Vec3 origin) {
  if (range.fade <= 0.0f) return range;
  const float apart = length(seen.sphere.center - origin);
  range.from = std::max(0.0f, range.from - apart);
  range.to += apart;
  return range;
}

// Whether an object is drawn as the view model: it, or an object it is under, says so.
bool in_view_model(const Scene& scene, const GameObject& object) {
  for (ObjectId id = object.id; id != kNoObject; id = scene.at(id).parent) {
    if (scene.at(id).view_model) return true;
  }
  return false;
}

// What an object's record says besides where it is (its shape keys come after).
render_math::InstanceLook look_of(const GameObject& object, float first_bone, const render_math::DrawnRange& range) {
  render_math::InstanceLook look{object.tint, first_bone};
  look.sway = object.sways ? render_math::kSways : render_math::kStill;
  fade_in_look(range, look);
  return look;
}

}  // namespace

bool Renderer::Impl::drawn_apart(const Scene& scene, const GameObject& object) const {
  return view_models_apart && in_view_model(scene, object);
}

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
  const std::optional<SkeletonPose> pose = pose_skeleton(scene, object, next_bone);
  const render_math::DrawnRange range = drawn_range_of(object);
  render_math::InstanceLook look = look_of(object, pose ? static_cast<float>(next_bone) : render_math::kNoBones, range);
  const std::size_t next = write_morphs(object, pose ? next_bone + pose->bones * kTexelsPerMatrix : next_bone, look);
  if (temporal.on) note_fresh_record(scene, object, look, pose ? pose->bones : 0);
  const render_math::Sight seen = reach_of(scene, object, pose);
  // A skinned mesh is placed by its bones alone: it is never taken as mirrored (KNOWN_LIMITATIONS).
  const bool mirrored = !pose && mirrors(worlds[slot]);
  const render_math::DrawnRange judged = judged_range(range, seen, worlds[slot].position);
  sights[slot] = ObjectSight{
      write_record(slot, look, range), seen, object.mesh, judged.from, judged.to, object.cast_shadow, mirrored};
  sights[slot].view_model = drawn_apart(scene, object);
  return next;
}

render_math::Sight Renderer::Impl::reach_of(const Scene& scene, const GameObject& object,
                                            const std::optional<SkeletonPose>& pose) const {
  const auto slot = static_cast<std::size_t>(object.id);
  const Bounds& bounds = mesh_bounds[static_cast<std::size_t>(object.mesh)];
  render_math::Sight seen = pose ? render_math::skinned_sight_of(bounds, worlds[slot], pose->first)
                                 : render_math::sight_of(bounds, worlds[slot]);
  seen.sphere.radius += mesh_wave_height(object.mesh) + water_ripple_bound(water_ripples, scene.handle(object.id));
  return seen;
}

std::size_t Renderer::Impl::write_morphs(const GameObject& object, std::size_t first, render_math::InstanceLook& look) {
  if (!render_math::shows_morphs(object.morph_weights)) return first;
  const render_math::MorphTexels pairs = render_math::morph_texels(render_math::active_morphs(object.morph_weights));
  if (!texels.write(first, pairs.data(), render_math::kMorphTexels)) return first;
  look.first_morph = static_cast<float>(first);
  return first + render_math::kMorphTexels;
}

render_math::DrawnRange Renderer::Impl::drawn_range_of(const GameObject& object) const {
  const auto slot = static_cast<std::size_t>(object.id);
  return render_math::drawn_range(object, render_math::longest_axis(worlds[slot].scale), view_tan_half_fov);
}

bool Renderer::Impl::write_record(std::size_t slot, const render_math::InstanceLook& look,
                                  const render_math::DrawnRange& range) {
  const std::size_t record = scattered_records + slot;
  const RecordSource source{
      worlds[slot], look.tint, look.first_bone, look.first_morph, look.sway, static_cast<float>(record), range};
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
  send_morph_texels();
}

// (Bound to its own slot even when no part has shape keys, which no shader reads then: a sampler
// left on slot 0 would share it with the material's picture.)
void Renderer::Impl::send_morph_texels() {
  bind_for_the_frame(shader, lit_uniforms.morph_texels, kMorphTexelSlot, morph_texture);
  bind_for_the_frame(depth_shader, depth_uniforms.morph_texels, kMorphTexelSlot, morph_texture);
  bind_for_the_frame(motion_shader, motion_uniforms.morph_texels, kMorphTexelSlot, morph_texture);
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
