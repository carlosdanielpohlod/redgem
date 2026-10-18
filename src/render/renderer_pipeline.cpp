// What lives as long as the window: loading it when the renderer is made, releasing it at the end.

#include "render/renderer_impl.hpp"

#include "render/shaders/ambient_occlusion.hpp"
#include "render/shaders/antialias.hpp"
#include "render/shaders/bloom.hpp"
#include "render/shaders/lit_fragment.hpp"
#include "render/shaders/lit_vertex.hpp"
#include "render/shaders/motion.hpp"
#include "render/shaders/panorama.hpp"
#include "render/shaders/particle.hpp"
#include "render/shaders/particle_glow.hpp"
#include "render/shaders/shadow_depth.hpp"
#include "render/shaders/sky.hpp"
#include "render/shaders/temporal.hpp"
#include "render/shaders/tone_mapping.hpp"

namespace redgem {
namespace {

using namespace render_detail;

// Sky dome (around the camera; its size only has to fit between the near and far planes).
constexpr float kSkyRadius = 50.0f;  // meters
constexpr int kSkyRings = 16;
constexpr int kSkySlices = 32;

// Tells a shader's sampler which texture slot it reads.
void assign_slot(const Shader& program, int sampler_location, int slot) {
  rlEnableShader(program.id);
  rlSetUniform(sampler_location, &slot, SHADER_UNIFORM_INT, 1);
  rlDisableShader();
}

// A material made by LoadMaterialDefault owns only its array of maps.
void free_maps(Material& material) {
  if (material.maps != nullptr) MemFree(material.maps);
  material = Material{};
}

}  // namespace

void Renderer::Impl::load_pipeline() {
  compile_shaders();
  compile_lit(shaders::LitFeatures{});
  depth_uniforms = find_depth_uniforms(depth_shader);
  depth_instance_slot = find_instance_slot(depth_shader);
  particle_uniforms = find_particle_uniforms(particle_shader);
  motion_uniforms = find_motion_uniforms(motion_shader);
  motion_instance_slot = find_instance_slot(motion_shader);
  temporal_uniforms = find_temporal_uniforms(temporal_shader);
  filter_cube_maps_across_seams();
  particle_buffer = load_particle_buffer(particle_uniforms.look);
  load_fixed_targets();
  fallback_material = make_material(redgem::Color{});
  load_sky();
}

void Renderer::Impl::compile_shaders() {
  depth_shader = compile_shader(shaders::lit_vertex().c_str(), shaders::depth_fragment().c_str(), "shadow depth");
  motion_shader = compile_shader(shaders::motion_vertex().c_str(), shaders::motion_fragment().c_str(), "motion");
  temporal_shader = compile_shader(nullptr, shaders::kTemporalFragment, "temporal anti-aliasing");
  sharpen_shader = compile_shader(nullptr, shaders::kSharpenFragment, "sharpening");
  sky_shader = compile_shader(shaders::kSkyVertex, shaders::kSkyFragment, "sky");
  particle_shader = compile_shader(shaders::kParticleVertex, shaders::kParticleFragment, "particles");
  glow_limit_shader = compile_shader(nullptr, shaders::kGlowLimitFragment, "glow limit");
  bright_shader = compile_shader(nullptr, shaders::kBrightFragment, "bloom threshold");
  blur_shader = compile_shader(nullptr, shaders::kBlurFragment, "blur");
  composite_shader = compile_shader(nullptr, shaders::kCompositeFragment, "tone mapping");
  antialias_shader = compile_shader(nullptr, shaders::kAntialiasFragment, "anti-aliasing");
  panorama_shader = compile_shader(nullptr, shaders::kPanoramaFragment, "reflection panorama");
  occlusion_shader = compile_shader(nullptr, shaders::kAmbientOcclusionFragment, "ambient occlusion");
}

void Renderer::Impl::compile_lit(const shaders::LitFeatures& features) {
  if (loaded(shader)) UnloadShader(shader);
  shader = compile_shader(shaders::lit_vertex().c_str(), shaders::lit_fragment(features).c_str(), "lit");
  lit_uniforms = find_lit_uniforms(shader);
  lit_instance_slot = find_instance_slot(shader);
  // A cube map's sampler must never share a slot with a picture's, even unused: given its own.
  assign_slot(shader, lit_uniforms.sky_reflections, kSkyReflectionSlot);
  assign_slot(shader, lit_uniforms.sky_reflectance, kSkyReflectanceSlot);
  for (Material& material : materials) material.shader = shader;
  fallback_material.shader = shader;
  lit_features = features;
}

void Renderer::Impl::use_lit_features(const shaders::LitFeatures& features) {
  if (features != lit_features) compile_lit(features);
}

void Renderer::Impl::load_sky() {
  sky_mesh = GenMeshSphere(kSkyRadius, kSkyRings, kSkySlices);
  sky_material = LoadMaterialDefault();
  sky_material.shader = sky_shader;
  sky_default_texture = sky_material.maps[MATERIAL_MAP_ALBEDO].texture;
}

void Renderer::Impl::release() {
  if (texel_texture != 0) rlUnloadTexture(texel_texture);
  texel_texture = 0;
  release_slots(slots);
  release_previous_texels();
  release_particle_buffer(particle_buffer);
  release_assets();
  release_targets();
  free_maps(fallback_material);
  release_sky();
  release_ui_images();
  if (owns_ui_font) UnloadFont(ui_font);
  owns_ui_font = false;
  release_fixed_targets();
  release_shaders();
}

void Renderer::Impl::release_sky() {
  release_sky_light();
  free_maps(sky_material);
  if (sky_mesh.vaoId != 0) UnloadMesh(sky_mesh);
  sky_mesh = Mesh{};
}

void Renderer::Impl::release_shaders() {
  for (Shader* program : {&shader, &depth_shader, &panorama_shader, &occlusion_shader, &sky_shader, &particle_shader,
                          &glow_limit_shader, &bright_shader, &blur_shader, &composite_shader, &antialias_shader,
                          &motion_shader, &temporal_shader, &sharpen_shader}) {
    if (loaded(*program)) UnloadShader(*program);
    *program = Shader{};
  }
}

}  // namespace redgem
