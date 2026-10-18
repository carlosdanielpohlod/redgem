#pragma once

// The engine's shaders on the graphics card: compiling them, finding where their uniforms are and
// sending values there.

#include <raylib.h>

namespace redgem::render_detail {

inline constexpr int kNoLocation = -1;  // a uniform the shader does not have

// Where the lit shader takes its values.
struct LitUniforms {
  // Per object and per material.
  int has_normal_map = kNoLocation;
  int normal_scale = kNoLocation;
  int has_surface_map = kNoLocation;
  int occlusion_strength = kNoLocation;
  int metallic = kNoLocation;
  int roughness = kNoLocation;
  int emissive = kNoLocation;
  int has_emissive_map = kNoLocation;
  int alpha_cutoff = kNoLocation;
  int alpha_to_coverage = kNoLocation;
  int alpha_ceiling = kNoLocation;
  int alpha_dithered = kNoLocation;
  int alpha_mixed = kNoLocation;
  int noise_shift = kNoLocation;
  int skinned = kNoLocation;
  int wind = kNoLocation;
  // Once per frame: the frame's matrices, lights, shadows, reflections and the sky.
  int frame_texels = kNoLocation;
  int shadow_map = kNoLocation;
  int light_matrix = kNoLocation;
  int shadow_strength = kNoLocation;
  int shadow_texel = kNoLocation;
  int reflection_map = kNoLocation;
  int reflection_strength = kNoLocation;
  int sun_direction = kNoLocation;
  int sun_color = kNoLocation;
  int ambient = kNoLocation;
  int point_count = kNoLocation;
  int point_positions = kNoLocation;
  int point_colors = kNoLocation;
  int point_ranges = kNoLocation;
  int fog_color = kNoLocation;
  int fog_density = kNoLocation;
  int time = kNoLocation;
  int sky_top = kNoLocation;
  int sky_horizon = kNoLocation;
  int ground = kNoLocation;
  int sky_irradiance = kNoLocation;
  int sky_reflections = kNoLocation;
  int sky_reflectance = kNoLocation;
  int shadow_tiles = kNoLocation;
  int shadow_scales = kNoLocation;
  int shadow_cascades = kNoLocation;
  int sun_size = kNoLocation;
};

// Where the shadow map's depth shader takes its values.
struct DepthUniforms {
  int alpha_cutoff = kNoLocation;
  int view_position = kNoLocation;
  int wind = kNoLocation;
  int time = kNoLocation;
  int skinned = kNoLocation;
  int frame_texels = kNoLocation;
};

// Where the motion pass's shader takes its values (see shaders/motion.hpp): per material, and
// once per frame.
struct MotionUniforms {
  int alpha_cutoff = kNoLocation;
  int alpha_ceiling = kNoLocation;
  int covering = kNoLocation;
  int alpha_dithered = kNoLocation;
  int wind = kNoLocation;
  int skinned = kNoLocation;
  int noise_shift = kNoLocation;
  int view_position = kNoLocation;
  int time = kNoLocation;
  int previous_time = kNoLocation;
  int frame_texels = kNoLocation;
  int previous_texels = kNoLocation;
  int view_projection = kNoLocation;
  int previous_view_projection = kNoLocation;
};

// Where the pass that mixes a frame with the ones before takes its values (see shaders/temporal.hpp).
struct TemporalUniforms {
  int history = kNoLocation;
  int motion = kNoLocation;
  int motion_depth = kNoLocation;
  int reprojection = kNoLocation;
  int texel_size = kNoLocation;
  int history_valid = kNoLocation;
};

// Where the particle shader takes its values: per batch, and once per frame (what soft ones fade
// into, and the light lit ones take).
struct ParticleUniforms {
  int boost = kNoLocation;
  int textured = kNoLocation;
  int scene_depth = kNoLocation;
  int near_plane = kNoLocation;
  int far_plane = kNoLocation;
  int sun_color = kNoLocation;
  int ambient = kNoLocation;
  int sky_top = kNoLocation;
  int sky_horizon = kNoLocation;
  int ground = kNoLocation;
  int point_count = kNoLocation;
  int point_positions = kNoLocation;
  int point_colors = kNoLocation;
  int point_ranges = kNoLocation;
  int look = kNoLocation;  // the vertex attribute of how soft and how lit a particle is
};

// A shader written for desktop OpenGL, compiled for this graphics card. `vertex` is null for a
// full-screen pass: raylib's default vertex shader does. Throws std::runtime_error, saying
// `name`, when it does not compile.
Shader compile_shader(const char* vertex, const char* fragment, const char* name);

// The lit one also tells raylib where the uniforms it sets by itself are (its texture maps).
LitUniforms find_lit_uniforms(Shader& lit);
DepthUniforms find_depth_uniforms(const Shader& depth);
MotionUniforms find_motion_uniforms(const Shader& motion);
TemporalUniforms find_temporal_uniforms(const Shader& temporal);
ParticleUniforms find_particle_uniforms(const Shader& particles);
// Where a shader takes the number of each copy's record (see instance_records.hpp).
int find_instance_slot(const Shader& program);

// Whether a shader has the uniform raylib keeps at `place` (one of its SHADER_LOC_ names).
inline bool takes(const Shader& shader, int place) { return shader.locs[place] != kNoLocation; }

// Sets one uniform of a shader, by location. (A shader without that uniform is left alone.)
inline void send(const Shader& shader, int location, float value) {
  SetShaderValue(shader, location, &value, SHADER_UNIFORM_FLOAT);
}
inline void send(const Shader& shader, int location, int value) {
  SetShaderValue(shader, location, &value, SHADER_UNIFORM_INT);
}
inline void send(const Shader& shader, int location, Vector2 value) {
  SetShaderValue(shader, location, &value, SHADER_UNIFORM_VEC2);
}
inline void send(const Shader& shader, int location, Vector3 value) {
  SetShaderValue(shader, location, &value, SHADER_UNIFORM_VEC3);
}
// The same, by name: for uniforms set too seldom to be worth remembering their location.
template <typename Value>
void send(const Shader& shader, const char* name, Value value) {
  send(shader, GetShaderLocation(shader, name), value);
}

}  // namespace redgem::render_detail
