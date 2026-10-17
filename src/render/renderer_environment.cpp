// What a scene is lit by and stands in: its lights, its atmosphere and the sky around it.

#include "render/renderer_impl.hpp"

#include <raymath.h>

#include <utility>

#include "render/scene_lights.hpp"
#include "render/shaders/uniforms.hpp"

namespace redgem {
namespace {

using namespace render_detail;
using render_math::to_raylib;

constexpr float kLightFadeDistance = 12.0f;  // meters over which the farthest used lights fade out

}  // namespace

void Renderer::Impl::apply_lights(const Scene& scene, const Environment& env, Vec3 viewer) {
  SceneLights lights = scene_lights(scene);
  const std::vector<PointLight> nearest =
      nearest_lights(std::move(lights.points), viewer, shaders::kMaxPointLights, kLightFadeDistance);
  const Vector3 sun_direction = to_raylib(lights.sun_direction);
  const Vector3 sun_color = to_raylib(lights.sun_color);
  const render_math::PointLightUniforms points = render_math::pack_point_lights(nearest);
  send_lights(sun_direction, sun_color, env.ambient, points);
  send_particle_lights(sun_color, env, points);
  send_atmosphere(env);
  send_sky_dome(env, sun_direction, sun_color);
  send_sky_light(env);
  wind = env.wind;
  glow_limit = env.glow_limit;
  sun_travel = sun_direction;
}

void Renderer::Impl::send_lights(Vector3 sun_direction, Vector3 sun_color, float ambient,
                                 const render_math::PointLightUniforms& points) {
  send(shader, lit_uniforms.sun_direction, sun_direction);
  send(shader, lit_uniforms.sun_color, sun_color);
  send(shader, lit_uniforms.ambient, ambient);
  send(shader, lit_uniforms.point_count, points.count);
  if (points.count == 0) return;
  SetShaderValueV(shader, lit_uniforms.point_positions, points.positions, SHADER_UNIFORM_VEC3, points.count);
  SetShaderValueV(shader, lit_uniforms.point_colors, points.colors, SHADER_UNIFORM_VEC3, points.count);
  SetShaderValueV(shader, lit_uniforms.point_ranges, points.ranges, SHADER_UNIFORM_FLOAT, points.count);
}

void Renderer::Impl::send_particle_lights(Vector3 sun_color, const Environment& env,
                                          const render_math::PointLightUniforms& points) {
  const ParticleUniforms& at = particle_uniforms;
  send(particle_shader, at.sun_color, sun_color);
  send(particle_shader, at.ambient, env.ambient);
  send(particle_shader, at.sky_top, to_raylib(env.sky_top));
  send(particle_shader, at.sky_horizon, to_raylib(env.sky_horizon));
  send(particle_shader, at.ground, to_raylib(env.ground));
  send(particle_shader, at.point_count, points.count);
  if (points.count == 0) return;
  SetShaderValueV(particle_shader, at.point_positions, points.positions, SHADER_UNIFORM_VEC3, points.count);
  SetShaderValueV(particle_shader, at.point_colors, points.colors, SHADER_UNIFORM_VEC3, points.count);
  SetShaderValueV(particle_shader, at.point_ranges, points.ranges, SHADER_UNIFORM_FLOAT, points.count);
}

// What surfaces are lit by from all around, what hides them far away, and the time foliage sways by.
void Renderer::Impl::send_atmosphere(const Environment& env) {
  send(shader, lit_uniforms.sky_top, to_raylib(env.sky_top));
  send(shader, lit_uniforms.sky_horizon, to_raylib(env.sky_horizon));
  send(shader, lit_uniforms.ground, to_raylib(env.ground));
  send(shader, lit_uniforms.fog_color, to_raylib(env.fog_color));
  send(shader, lit_uniforms.fog_density, env.fog_density);
  send(shader, lit_uniforms.time, now());
}

// The sky dome shows the same sky the surfaces are lit by.
void Renderer::Impl::send_sky_dome(const Environment& env, Vector3 sun_direction, Vector3 sun_color) {
  send(sky_shader, shaders::kSkyTop, to_raylib(env.sky_top));
  send(sky_shader, shaders::kSkyHorizon, to_raylib(env.sky_horizon));
  send(sky_shader, shaders::kGroundColor, to_raylib(env.ground));
  send(sky_shader, shaders::kSunDirection, sun_direction);
  send(sky_shader, shaders::kSunColor, sun_color);
  send(sky_shader, shaders::kSunGlow, env.sun_glow);
}

void Renderer::Impl::draw_sky(const Camera3D& camera) {
  rlDisableBackfaceCulling();
  rlDisableDepthMask();
  rlDisableDepthTest();
  DrawMesh(sky_mesh, sky_material, MatrixTranslate(camera.position.x, camera.position.y, camera.position.z));
  rlEnableDepthTest();
  rlEnableDepthMask();
  rlEnableBackfaceCulling();
}

}  // namespace redgem
