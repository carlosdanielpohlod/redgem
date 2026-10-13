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
  int tint = kNoLocation;
  int skinned = kNoLocation;
  int wind = kNoLocation;
  // Once per frame: lights, shadows, reflections and the sky.
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
};

// Where the shadow map's depth shader takes its values.
struct DepthUniforms {
  int alpha_cutoff = kNoLocation;
  int wind = kNoLocation;
  int time = kNoLocation;
  int skinned = kNoLocation;
};

struct ParticleUniforms {
  int boost = kNoLocation;
  int textured = kNoLocation;
};

// A shader written for desktop OpenGL, compiled for this graphics card. `vertex` is null for a
// full-screen pass: raylib's default vertex shader does. Throws std::runtime_error, saying
// `name`, when it does not compile.
Shader compile_shader(const char* vertex, const char* fragment, const char* name);

// Each also tells raylib where the uniforms it sets by itself are (matrices, texture maps, bones).
LitUniforms find_lit_uniforms(Shader& lit);
DepthUniforms find_depth_uniforms(Shader& depth);
ParticleUniforms find_particle_uniforms(const Shader& particles);

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
