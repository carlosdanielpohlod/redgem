#include "render/shader_programs.hpp"

#include <stdexcept>
#include <string>
#include <utility>

#include "render/shader_source.hpp"
#include "render/mesh_pass.hpp"
#include "render/shaders/motion.hpp"
#include "render/shaders/particle.hpp"
#include "render/shaders/temporal.hpp"
#include "render/shaders/uniforms.hpp"

namespace redgem::render_detail {
namespace {

// The uniforms raylib has a place for, under the lit shader's own names for them.
constexpr std::pair<int, const char*> kKnownUniforms[] = {
    {SHADER_LOC_VECTOR_VIEW, shaders::kViewPosition},
    {SHADER_LOC_MAP_EMISSION, shaders::kEmissiveMap},
    {SHADER_LOC_MAP_NORMAL, shaders::kNormalMap},
    {SHADER_LOC_MAP_ROUGHNESS, shaders::kSurfaceMap},
    {SHADER_LOC_MAP_OCCLUSION, shaders::kOcclusionMap},
    {SHADER_LOC_MAP_DIFFUSE + kCoatFactorMap, shaders::kCoatFactorMap},
    {SHADER_LOC_MAP_DIFFUSE + kCoatRoughnessMap, shaders::kCoatRoughnessMap},
    {SHADER_LOC_MAP_DIFFUSE + kCoatNormalMap, shaders::kCoatNormalMap},
};

constexpr std::pair<int ShadowUniforms::*, const char*> kShadowUniforms[] = {
    {&ShadowUniforms::map, shaders::kShadowMap},
    {&ShadowUniforms::matrices, shaders::kLightViewProjection},
    {&ShadowUniforms::strength, shaders::kShadowStrength},
    {&ShadowUniforms::texel, shaders::kShadowTexel},
    {&ShadowUniforms::tiles, shaders::kShadowTiles},
    {&ShadowUniforms::scales, shaders::kShadowScales},
    {&ShadowUniforms::cascades, shaders::kShadowCascades},
    {&ShadowUniforms::sun_size, shaders::kSunSize},
    {&ShadowUniforms::depth_range, shaders::kShadowDepthRange},
};

// A uniform of the lit shader, and where its location is kept.
struct LitUniform {
  int LitUniforms::* location;
  const char* name;
};

constexpr LitUniform kLitUniforms[] = {
    // Per object and per material.
    {&LitUniforms::water_ready, shaders::kWaterReady},
    {&LitUniforms::water_absorption, shaders::kWaterAbsorption},
    {&LitUniforms::water_surface, shaders::kWaterSurface},
    {&LitUniforms::water_planes, shaders::kWaterPlanes},
    {&LitUniforms::water_background, shaders::kWaterBackground},
    {&LitUniforms::water_depth, shaders::kWaterDepth},
    {&LitUniforms::coat_factor, shaders::kCoatFactor},
    {&LitUniforms::coat_roughness, shaders::kCoatRoughness},
    {&LitUniforms::has_coat_factor_map, shaders::kHasCoatFactorMap},
    {&LitUniforms::has_coat_roughness_map, shaders::kHasCoatRoughnessMap},
    {&LitUniforms::has_coat_normal_map, shaders::kHasCoatNormalMap},
    {&LitUniforms::coat_normal_scale, shaders::kCoatNormalScale},
    {&LitUniforms::has_normal_map, shaders::kHasNormalMap},
    {&LitUniforms::normal_scale, shaders::kNormalScale},
    {&LitUniforms::has_surface_map, shaders::kHasSurfaceMap},
    {&LitUniforms::occlusion_strength, shaders::kOcclusionStrength},
    {&LitUniforms::metallic, shaders::kMetallic},
    {&LitUniforms::roughness, shaders::kRoughness},
    {&LitUniforms::emissive, shaders::kEmissive},
    {&LitUniforms::has_emissive_map, shaders::kHasEmissiveMap},
    {&LitUniforms::alpha_cutoff, shaders::kAlphaCutoff},
    {&LitUniforms::alpha_to_coverage, shaders::kAlphaToCoverage},
    {&LitUniforms::alpha_ceiling, shaders::kAlphaCeiling},
    {&LitUniforms::alpha_dithered, shaders::kAlphaDithered},
    {&LitUniforms::noise_shift, shaders::kNoiseShift},
    {&LitUniforms::alpha_mixed, shaders::kAlphaMixed},
    {&LitUniforms::skinned, shaders::kSkinned},
    {&LitUniforms::wind, shaders::kWindAmount},
    // Once per frame: the frame's matrices, lights, shadows, reflections and the sky.
    {&LitUniforms::frame_texels, shaders::kFrameTexels},
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
    {&LitUniforms::sky_irradiance, shaders::kSkyIrradiance},
    {&LitUniforms::sky_reflections, shaders::kSkyReflections},
    {&LitUniforms::sky_reflectance, shaders::kSkyReflectance},
};

// A uniform of the particle shader, and where its location is kept.
struct ParticleUniform {
  int ParticleUniforms::* location;
  const char* name;
};

constexpr ParticleUniform kParticleUniforms[] = {
    {&ParticleUniforms::sun_direction, shaders::kSunDirection},
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

// The motion pass's uniforms, and where each one's location is kept.
constexpr std::pair<int MotionUniforms::*, const char*> kMotionUniforms[] = {
    {&MotionUniforms::alpha_cutoff, shaders::kAlphaCutoff},
    {&MotionUniforms::alpha_ceiling, shaders::kAlphaCeiling},
    {&MotionUniforms::covering, shaders::kCovering},
    {&MotionUniforms::alpha_dithered, shaders::kAlphaDithered},
    {&MotionUniforms::wind, shaders::kWindAmount},
    {&MotionUniforms::skinned, shaders::kSkinned},
    {&MotionUniforms::noise_shift, shaders::kNoiseShift},
    {&MotionUniforms::view_position, shaders::kViewPosition},
    {&MotionUniforms::time, shaders::kTime},
    {&MotionUniforms::previous_time, shaders::kPreviousTime},
    {&MotionUniforms::frame_texels, shaders::kFrameTexels},
    {&MotionUniforms::previous_texels, shaders::kPreviousTexels},
    {&MotionUniforms::view_projection, shaders::kViewProjection},
    {&MotionUniforms::previous_view_projection, shaders::kPreviousViewProjection},
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

ShadowUniforms find_shadow_uniforms(const Shader& program) {
  ShadowUniforms found;
  for (const auto& [member, name] : kShadowUniforms) found.*member = GetShaderLocation(program, name);
  return found;
}

LitUniforms find_lit_uniforms(Shader& lit) {
  for (const auto& [place, name] : kKnownUniforms) lit.locs[place] = GetShaderLocation(lit, name);
  LitUniforms found;
  found.shadows = find_shadow_uniforms(lit);
  for (const LitUniform& uniform : kLitUniforms) found.*uniform.location = GetShaderLocation(lit, uniform.name);
  return found;
}

WaterUniforms find_water_uniforms(const Shader& program) {
  WaterUniforms found;
  found.enabled = GetShaderLocation(program, shaders::kWaterEnabled);
  found.waves = GetShaderLocation(program, shaders::kWaterWaveParameters);
  found.ripples = {GetShaderLocation(program, shaders::kWaterRippleCount),
                   GetShaderLocation(program, shaders::kWaterRipples),
                   GetShaderLocation(program, shaders::kWaterRadii)};
  found.previous_ripples = {GetShaderLocation(program, shaders::kPreviousWaterRippleCount),
                            GetShaderLocation(program, shaders::kPreviousWaterRipples),
                            GetShaderLocation(program, shaders::kPreviousWaterRadii)};
  return found;
}

DepthUniforms find_depth_uniforms(const Shader& depth) {
  DepthUniforms found;
  found.alpha_cutoff = GetShaderLocation(depth, shaders::kAlphaCutoff);
  found.view_position = GetShaderLocation(depth, shaders::kViewPosition);
  found.wind = GetShaderLocation(depth, shaders::kWindAmount);
  found.time = GetShaderLocation(depth, shaders::kTime);
  found.skinned = GetShaderLocation(depth, shaders::kSkinned);
  found.frame_texels = GetShaderLocation(depth, shaders::kFrameTexels);
  return found;
}

MotionUniforms find_motion_uniforms(const Shader& motion) {
  MotionUniforms found;
  for (const auto& [location, name] : kMotionUniforms) found.*location = GetShaderLocation(motion, name);
  return found;
}

TemporalUniforms find_temporal_uniforms(const Shader& temporal) {
  TemporalUniforms found;
  found.history = GetShaderLocation(temporal, shaders::kHistoryTexture);
  found.motion = GetShaderLocation(temporal, shaders::kMotionTexture);
  found.motion_depth = GetShaderLocation(temporal, shaders::kMotionDepth);
  found.reprojection = GetShaderLocation(temporal, shaders::kReprojection);
  found.texel_size = GetShaderLocation(temporal, shaders::kTexelSize);
  found.history_valid = GetShaderLocation(temporal, shaders::kHistoryValid);
  found.depth_projection = GetShaderLocation(temporal, shaders::kDepthProjection);
  found.current_jitter = GetShaderLocation(temporal, shaders::kCurrentJitter);
  found.previous_jitter = GetShaderLocation(temporal, shaders::kPreviousJitter);
  return found;
}

int find_instance_slot(const Shader& program) { return GetShaderLocationAttrib(program, shaders::kInstanceSlot); }

ParticleUniforms find_particle_uniforms(const Shader& particles) {
  ParticleUniforms found;
  found.shadows = find_shadow_uniforms(particles);
  for (const ParticleUniform& uniform : kParticleUniforms) {
    found.*uniform.location = GetShaderLocation(particles, uniform.name);
  }
  found.look = GetShaderLocationAttrib(particles, shaders::kParticleLook);
  return found;
}

}  // namespace redgem::render_detail
