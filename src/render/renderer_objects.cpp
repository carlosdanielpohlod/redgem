// Drawing the scene's objects: each one's materials, and the parts of its mesh that the view can
// see. (Where each one is and its bones were worked out for the frame: see renderer_frame_texels.)

#include "render/renderer_impl.hpp"

#include <raymath.h>

#include <algorithm>

#include "render/culling.hpp"

namespace redgem {
namespace {

using namespace render_detail;
using render_math::kOff;
using render_math::kOn;
using render_math::model_matrix;
using render_math::to_raylib;

// A switch of the lit shader: on for a material that has that texture.
float on_with(int image) { return image != kNoIndex ? kOn : kOff; }

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
  const std::size_t count = std::min({scene.size(), worlds.size(), sights.size()});
  ObjectPass objects{begin_mesh_pass(depth_pass ? depth_shader : shader),
                     depth_pass ? depth_uniforms.skinned : lit_uniforms.skinned,
                     depth_pass ? depth_uniforms.first_bone : lit_uniforms.first_bone, depth_pass, view};
  for (std::size_t slot = 0; slot < count; ++slot) {
    if (!sights[slot].drawn) continue;
    draw_object(scene.at(static_cast<ObjectId>(slot)), worlds[slot], sights[slot], objects);
  }
  end_mesh_pass();
}

// Not what the view cannot see, what looks too small in it, nor levels of detail meant for
// another distance.
void Renderer::Impl::draw_object(const GameObject& object, const Transform& world, const ObjectSight& sight,
                                 ObjectPass& pass) {
  if (!render_math::sighted(pass.view, sight.sight, object.visible_from, object.visible_to)) return;
  const Matrix transform = model_matrix(world);
  if (!pass.depth) {
    const Vector3 tint = to_raylib(object.tint);
    rlSetUniform(lit_uniforms.tint, &tint, SHADER_UNIFORM_VEC3, 1);
  }
  for (const GpuPrimitive& primitive : meshes[static_cast<std::size_t>(object.mesh)]) {
    draw_primitive(primitive, transform, sight.first_bone, pass);
  }
}

void Renderer::Impl::draw_primitive(const GpuPrimitive& primitive, const Matrix& transform, float first_bone,
                                    ObjectPass& pass) {
  use_material(primitive.material, pass);
  const bool two_sided = pass.last_two_sided;
  if (two_sided) rlDisableBackfaceCulling();
  // Per part: a skinned object may still have parts without bone weights, which stay rigid.
  const bool deformed = render_math::has_bones(first_bone) && primitive.skinned;
  const float skinning = deformed ? kOn : kOff;
  rlSetUniform(pass.skinned_location, &skinning, SHADER_UNIFORM_FLOAT, 1);
  if (deformed) rlSetUniform(pass.first_bone_location, &first_bone, SHADER_UNIFORM_FLOAT, 1);
  draw_mesh(pass.meshes, primitive.mesh, transform);
  if (two_sided) rlEnableBackfaceCulling();
}

}  // namespace redgem
