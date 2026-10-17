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

// Whether a view draws an object: one the frame has a record of, in the view, big enough in it,
// and at the distance its level of detail is for.
bool seen(const ObjectSight& sight, const GameObject& object, const render_math::View& view) {
  return sight.drawn && render_math::sighted(view, sight.sight, object.visible_from, object.visible_to);
}

}  // namespace

// --- Materials --------------------------------------------------------------------------------

const MaterialDesc& Renderer::Impl::factors_of(int material) const {
  const bool known = is_index_of(material, material_factors.size());
  return known ? material_factors[static_cast<std::size_t>(material)] : fallback_factors;
}

bool Renderer::Impl::apply_material(int index, bool depth_pass) {
  const MaterialDesc& factors = factors_of(index);
  const render_math::AlphaTest test = render_math::alpha_test(factors, wind);
  const Shader& pass_shader = depth_pass ? depth_shader : shader;
  if (!depth_pass) send_surface(factors);
  send(pass_shader, depth_pass ? depth_uniforms.alpha_cutoff : lit_uniforms.alpha_cutoff, test.cutoff);
  send(pass_shader, depth_pass ? depth_uniforms.wind : lit_uniforms.wind, test.sway);
  return test.two_sided;
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

// Consecutive parts often share a material: its textures and uniforms are then already set.
void Renderer::Impl::use_material(int material, ObjectPass& pass) {
  if (material == pass.last_material) return;
  pass.last_two_sided = apply_material(material, pass.depth);
  bind_material(pass.meshes, material == kNoIndex ? fallback_material : materials[static_cast<std::size_t>(material)]);
  pass.last_material = material;
}

// --- Objects ----------------------------------------------------------------------------------

void Renderer::Impl::draw_objects(const Scene& scene, bool depth_pass, Vec3 viewer, float detail_angle) {
  const Matrix view_projection = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
  const render_math::View view{frustum_from_matrix(MatrixToFloatV(view_projection).v), viewer,
                               render_math::smallest_angle_drawn(detail_angle, depth_pass)};
  gather_copies(scene, view);
  const std::size_t first = append_slots(slots, batches.slots());
  ObjectPass objects{
      begin_mesh_pass(depth_pass ? depth_shader : shader, depth_pass ? depth_instance_slot : lit_instance_slot),
      depth_pass ? depth_uniforms.skinned : lit_uniforms.skinned, depth_pass, view};
  for (const render_math::InstanceBatch& batch : batches.batches()) draw_batch(batch, first, objects);
  end_mesh_pass();
}

void Renderer::Impl::gather_copies(const Scene& scene, const render_math::View& view) {
  batches.start(meshes.size());
  const std::size_t count = std::min(scene.size(), sights.size());
  for (std::size_t slot = 0; slot < count; ++slot) {
    const GameObject& object = scene.at(static_cast<ObjectId>(slot));
    if (seen(sights[slot], object, view)) batches.add(object.mesh, slot);
  }
  batches.lay_out();
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
