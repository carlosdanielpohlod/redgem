// Drawing the scene's objects: what the view can see of them, every copy of a mesh in one call per
// part, and their materials. (Where each one is, its look and its bones were worked out for the
// frame: see renderer_frame_texels.)

#include "render/renderer_impl.hpp"

#include <raymath.h>

#include <algorithm>

#include "render/culling.hpp"

namespace redgem {
namespace {

using namespace render_detail;
using render_math::kOff;
using render_math::kOn;
using render_math::to_raylib;

// A switch of the lit shader: on for a material that has that texture.
float on_with(int image) { return image != kNoIndex ? kOn : kOff; }

// Whether a view draws an object: one the frame has a record of, that casts a shadow when the
// pass is the shadow map's, in the view, big enough in it, and at the distance its level of
// detail is for.
bool seen(const ObjectSight& sight, const render_math::View& view, bool depth_pass) {
  return sight.drawn && casts_into(sight.cast_shadow, depth_pass) &&
         render_math::sighted(view, sight.sight, sight.visible_from, sight.visible_to);
}

// Whether the picture's samples smooth a material's cut-out edges (alpha to coverage): one cut out
// by a cutoff, not a dithered one or one mixed with what is behind it.
bool smooths_edges(const render_math::AlphaTest& test) {
  return test.cutoff > render_math::kNoCutoff && !test.dithered && !test.mixed;
}

// Whether a pass draws for temporal anti-aliasing: into the motion target, with the motion shader.
bool for_temporal(render_math::AlphaPass pass) {
  return pass == render_math::AlphaPass::Motion || pass == render_math::AlphaPass::Cover;
}

}  // namespace

// --- Materials --------------------------------------------------------------------------------

const MaterialDesc& Renderer::Impl::factors_of(int material) const {
  const bool known = is_index_of(material, material_factors.size());
  return known ? material_factors[static_cast<std::size_t>(material)] : fallback_factors;
}

bool Renderer::Impl::apply_material(int index, render_math::AlphaPass pass) {
  const MaterialDesc& factors = factors_of(index);
  const render_math::AlphaTest test = render_math::alpha_test(factors, wind, pass);
  if (pass == render_math::AlphaPass::Shadow) {
    send(depth_shader, depth_uniforms.alpha_cutoff, test.cutoff);
    send(depth_shader, depth_uniforms.wind, test.sway);
  } else if (for_temporal(pass)) {
    send_motion_alpha(test, pass == render_math::AlphaPass::Cover);
  } else {
    send_surface(factors);
    cover_edges(smooths_edges(test));
    send_alpha(test);
  }
  return test.two_sided;
}

// What of a material is drawn in the lit pass, and how it sways.
void Renderer::Impl::send_alpha(const render_math::AlphaTest& test) {
  send(shader, lit_uniforms.alpha_cutoff, test.cutoff);
  send(shader, lit_uniforms.alpha_ceiling, test.ceiling);
  send(shader, lit_uniforms.alpha_dithered, test.dithered ? kOn : kOff);
  send(shader, lit_uniforms.alpha_mixed, test.mixed ? kOn : kOff);
  send(shader, lit_uniforms.wind, test.sway);
}

// What of a material the motion pass leaves out, and how it sways: as the lit pass.
void Renderer::Impl::send_motion_alpha(const render_math::AlphaTest& test, bool covering) {
  send(motion_shader, motion_uniforms.covering, covering ? kOn : kOff);
  send(motion_shader, motion_uniforms.alpha_ceiling, test.ceiling);
  send(motion_shader, motion_uniforms.alpha_cutoff, test.cutoff);
  send(motion_shader, motion_uniforms.alpha_dithered, test.dithered ? kOn : kOff);
  send(motion_shader, motion_uniforms.wind, test.sway);
}

// How a material's surface answers light.
void Renderer::Impl::send_surface(const MaterialDesc& factors) {
  const bool has_occlusion_map = factors.occlusion_image != kNoIndex;
  send(shader, lit_uniforms.has_normal_map, on_with(factors.normal_image));
  send(shader, lit_uniforms.normal_scale, factors.normal_scale);
  send(shader, lit_uniforms.has_surface_map, on_with(factors.metallic_roughness_image));
  send(shader, lit_uniforms.occlusion_strength, has_occlusion_map ? factors.occlusion_strength : kOff);
  send(shader, lit_uniforms.metallic, factors.metallic);
  send(shader, lit_uniforms.roughness, factors.roughness);
  send(shader, lit_uniforms.emissive, to_raylib(factors.emissive));
  send(shader, lit_uniforms.has_emissive_map, on_with(factors.emissive_image));
}

// With several samples a pixel, a cut-out material's edges are smoothed by them (alpha to
// coverage): its alpha then says how much of a pixel it covers, and does not blend.
void Renderer::Impl::cover_edges(bool cut_out) {
  const bool on = covering && cut_out;
  send(shader, lit_uniforms.alpha_to_coverage, on ? kOn : kOff);
  if (!covering) return;
  set_alpha_to_coverage(on);
  if (on) {
    rlDisableColorBlend();
  } else {
    rlEnableColorBlend();
  }
}

// Consecutive parts often share a material: its textures and uniforms are then already set.
void Renderer::Impl::use_material(int material, ObjectPass& pass) {
  if (material == pass.last_material) return;
  pass.last_two_sided = apply_material(material, pass.alpha);
  bind_material(pass.meshes, material == kNoIndex ? fallback_material : materials[static_cast<std::size_t>(material)]);
  pass.last_material = material;
}

// --- Objects ----------------------------------------------------------------------------------

std::size_t Renderer::Impl::draw_objects(bool depth_pass, Vec3 viewer, float detail_angle) {
  const Matrix view_projection = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
  const render_math::View view{frustum_from_matrix(MatrixToFloatV(view_projection).v), viewer,
                               render_math::smallest_angle_drawn(detail_angle, depth_pass)};
  gather_copies(view, depth_pass);
  const std::size_t first = append_slots(slots, batches.slots());
  ObjectPass objects{
      begin_mesh_pass(depth_pass ? depth_shader : shader, depth_pass ? depth_instance_slot : lit_instance_slot),
      depth_pass ? depth_uniforms.skinned : lit_uniforms.skinned,
      depth_pass ? render_math::AlphaPass::Shadow : render_math::AlphaPass::Opaque, view};
  for (const render_math::InstanceBatch& batch : batches.batches()) draw_batch(batch, first, objects);
  if (covering) cover_edges(false);
  end_mesh_pass();
  return first;
}

void Renderer::Impl::gather_copies(const render_math::View& view, bool depth_pass) {
  batches.start();
  for (std::size_t slot = 0; slot < sights.size(); ++slot) {
    if (seen(sights[slot], view, depth_pass)) batches.add(sights[slot].mesh, scattered_records + slot);
  }
  gather_scattered(view, depth_pass);
}

void Renderer::Impl::draw_batch(const render_math::InstanceBatch& batch, std::size_t first, ObjectPass& pass) {
  const InstanceRun run{slots.id, first + batch.first, batch.count};
  for (const GpuPrimitive& primitive : meshes[static_cast<std::size_t>(batch.mesh)])
    draw_primitive(primitive, run, pass);
}

void Renderer::Impl::draw_primitive(const GpuPrimitive& primitive, const InstanceRun& run, ObjectPass& pass) {
  use_material(primitive.material, pass);
  const bool two_sided = pass.last_two_sided;
  if (two_sided) rlDisableBackfaceCulling();
  // Per part: a skinned object may still have parts without bone weights, which stay rigid. (So
  // does a copy whose skeleton could not be posed: it has no first bone.)
  const float skinning = primitive.skinned ? kOn : kOff;
  rlSetUniform(pass.skinned_location, &skinning, SHADER_UNIFORM_FLOAT, 1);
  draw_instances(pass.meshes, primitive.mesh, run);
  if (two_sided) rlEnableBackfaceCulling();
}

}  // namespace redgem
