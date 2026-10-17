#include "render/shader_programs.hpp"

#include <stdexcept>
#include <string>
#include <utility>

#include "render/shader_source.hpp"
#include "render/shaders/particle.hpp"
#include "render/shaders/uniforms.hpp"

namespace redgem::render_detail {
namespace {

// The uniforms raylib has a place for, under the lit shader's own names for them.
constexpr std::pair<int, const char*> kKnownUniforms[] = {
    {SHADER_LOC_VECTOR_VIEW, shaders::kViewPosition},   {SHADER_LOC_MAP_EMISSION, shaders::kEmissiveMap},
    {SHADER_LOC_MAP_NORMAL, shaders::kNormalMap},       {SHADER_LOC_MAP_ROUGHNESS, shaders::kSurfaceMap},
    {SHADER_LOC_MAP_OCCLUSION, shaders::kOcclusionMap},
};

// A uniform of the lit shader, and where its location is kept.
struct LitUniform {
  int LitUniforms::* location;
  const char* name;
};

constexpr LitUniform kLitUniforms[] = {
    // Per object and per material.
    {&LitUniforms::has_normal_map, shaders::kHasNormalMap},
    {&LitUniforms::normal_scale, shaders::kNormalScale},
    {&LitUniforms::has_surface_map, shaders::kHasSurfaceMap},
    {&LitUniforms::occlusion_strength, shaders::kOcclusionStrength},
    {&LitUniforms::metallic, shaders::kMetallic},
    {&LitUniforms::roughness, shaders::kRoughness},
    {&LitUniforms::emissive, shaders::kEmissive},
    {&LitUniforms::has_emissive_map, shaders::kHasEmissiveMap},
    {&LitUniforms::alpha_cutoff, shaders::kAlphaCutoff},
    {&LitUniforms::skinned, shaders::kSkinned},
    {&LitUniforms::wind, shaders::kWindAmount},
    // Once per frame: the frame's matrices, lights, shadows, reflections and the sky.
    {&LitUniforms::frame_texels, shaders::kFrameTexels},
    {&LitUniforms::shadow_map, shaders::kShadowMap},
    {&LitUniforms::light_matrix, shaders::kLightViewProjection},
    {&LitUniforms::shadow_strength, shaders::kShadowStrength},
    {&LitUniforms::shadow_texel, shaders::kShadowTexel},
    {&LitUniforms::reflection_map, shaders::kReflectionMap},
    {&LitUniforms::reflection_strength, shaders::kReflectionStrength},
    {&LitUniforms::sun_direction, shaders::kSunDirection},
    {&LitUniforms::sun_color, shaders::kSunColor},
    {&LitUniforms::ambient, shaders::kAmbientStrength},
    {&LitUniforms::point_count, shaders::kPointLightCount},
    {&LitUniforms::point_positions, shaders::kPointLightPositions},
    {&LitUniforms::point_colors, shaders::kPointLightColors},
    {&LitUniforms::point_ranges, shaders::kPointLightRanges},
    {&LitUniforms::fog_color, shaders::kFogColor},
    {&LitUniforms::fog_density, shaders::kFogDensity},
    {&LitUniforms::time, shaders::kTime},
    {&LitUniforms::sky_top, shaders::kSkyTop},
    {&LitUniforms::sky_horizon, shaders::kSkyHorizon},
    {&LitUniforms::ground, shaders::kGroundColor},
};

// A uniform of the particle shader, and where its location is kept.
struct ParticleUniform {
  int ParticleUniforms::* location;
  const char* name;
};

constexpr ParticleUniform kParticleUniforms[] = {
    {&ParticleUniforms::boost, shaders::kParticleBoost},
    {&ParticleUniforms::textured, shaders::kParticleTextured},
    {&ParticleUniforms::scene_depth, shaders::kSceneDepth},
    {&ParticleUniforms::near_plane, shaders::kNearPlane},
    {&ParticleUniforms::far_plane, shaders::kFarPlane},
    {&ParticleUniforms::sun_color, shaders::kSunColor},
    {&ParticleUniforms::ambient, shaders::kAmbientStrength},
    {&ParticleUniforms::sky_top, shaders::kSkyTop},
    {&ParticleUniforms::sky_horizon, shaders::kSkyHorizon},
    {&ParticleUniforms::ground, shaders::kGroundColor},
    {&ParticleUniforms::point_count, shaders::kPointLightCount},
    {&ParticleUniforms::point_positions, shaders::kPointLightPositions},
    {&ParticleUniforms::point_colors, shaders::kPointLightColors},
    {&ParticleUniforms::point_ranges, shaders::kPointLightRanges},
};

}  // namespace

Shader compile_shader(const char* vertex, const char* fragment, const char* name) {
  const std::string vertex_text = vertex != nullptr ? shaders::for_this_gpu(vertex) : std::string();
  const std::string fragment_text = shaders::for_this_gpu(fragment);
  const Shader compiled =
      LoadShaderFromMemory(vertex != nullptr ? vertex_text.c_str() : nullptr, fragment_text.c_str());
  if (!IsShaderValid(compiled)) throw std::runtime_error(std::string("failed to compile the ") + name + " shader");
  return compiled;
}

LitUniforms find_lit_uniforms(Shader& lit) {
  for (const auto& [place, name] : kKnownUniforms) lit.locs[place] = GetShaderLocation(lit, name);
  LitUniforms found;
  for (const LitUniform& uniform : kLitUniforms) found.*uniform.location = GetShaderLocation(lit, uniform.name);
  return found;
}

DepthUniforms find_depth_uniforms(const Shader& depth) {
  DepthUniforms found;
  found.alpha_cutoff = GetShaderLocation(depth, shaders::kAlphaCutoff);
  found.wind = GetShaderLocation(depth, shaders::kWindAmount);
  found.time = GetShaderLocation(depth, shaders::kTime);
  found.skinned = GetShaderLocation(depth, shaders::kSkinned);
  found.frame_texels = GetShaderLocation(depth, shaders::kFrameTexels);
  return found;
}

int find_instance_slot(const Shader& program) { return GetShaderLocationAttrib(program, shaders::kInstanceSlot); }

ParticleUniforms find_particle_uniforms(const Shader& particles) {
  ParticleUniforms found;
  for (const ParticleUniform& uniform : kParticleUniforms) {
    found.*uniform.location = GetShaderLocation(particles, uniform.name);
  }
  found.look = GetShaderLocationAttrib(particles, shaders::kParticleLook);
  return found;
}

}  // namespace redgem::render_detail
