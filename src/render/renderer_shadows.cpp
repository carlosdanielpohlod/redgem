// The sun's shadow map: the scene's depth as the sun sees it, for the lit shader to look up. One
// picture around where the camera looks, or several cascades (shadow_cascades.hpp), each a tile
// of the map.

#include "render/renderer_impl.hpp"

#include <raymath.h>

#include <array>
#include <cmath>

#include "render/shaders/uniforms.hpp"

namespace redgem {
namespace {

using namespace render_detail;
using render_math::Cascade;
using render_math::Cascades;
using render_math::kMaxShadowCascades;
using render_math::kShadowMapSize;
using render_math::to_vec3;

// The camera as the cascades see it.
render_math::CascadeCamera cascade_camera(const Camera3D& camera, float tan_half_fov, float aspect) {
  return {camera.position, camera.target, Vector3Normalize(Vector3Subtract(camera.target, camera.position)),
          tan_half_fov,    aspect,        static_cast<float>(rlGetCullDistanceNear())};
}

// The sun's view only needs the depth range that can hold casters and receivers; a tight range
// keeps depth precise (and the shadow bias small).
constexpr double kShadowNearPlane = 1.0;                                   // meters
constexpr float kShadowTexel = 1.0f / static_cast<float>(kShadowMapSize);  // in texture coordinates
constexpr int kFloatsPerTile = 4;                                          // its corner and its size, twice

// Meters: as far past the cascades' middle as the sun is before it.
double shadow_far_plane(const Cascades& cascades) { return kTwo * static_cast<double>(cascades.sun_distance); }

// The sun's point of view: straight (no perspective) and wide enough for everything shadowed.
Camera3D sun_camera(const Cascade& cascade) {
  Camera3D sun_view{};
  sun_view.projection = CAMERA_ORTHOGRAPHIC;
  sun_view.fovy = cascade.extent;
  sun_view.target = cascade.view.target;
  sun_view.position = cascade.view.position;
  sun_view.up = cascade.view.up;
  return sun_view;
}

int pixels_of(float uv) { return static_cast<int>(std::lround(uv * static_cast<float>(kShadowMapSize))); }

// How far the sun is across, as the lit shader takes it: the tangent of its apparent radius.
float sun_size_of(const Environment& env) { return std::tan(env.shadow_softness * kHalf); }

// Each cascade's tile (its corner, then its size twice) and texel scale, as the lit shader's arrays.
struct CascadeUniforms {
  std::array<float, kMaxShadowCascades * kFloatsPerTile> tiles{};
  std::array<float, kMaxShadowCascades> scales{};
};

CascadeUniforms cascade_uniforms(const Cascades& cascades) {
  CascadeUniforms values;
  for (std::size_t i = 0; i < static_cast<std::size_t>(cascades.count); ++i) {
    const Cascade& cascade = cascades.cascades[i];
    float* tile = &values.tiles[i * kFloatsPerTile];
    tile[kXOffset] = cascade.corner.x;
    tile[kYOffset] = cascade.corner.y;
    tile[kZOffset] = cascade.size;
    tile[kWOffset] = cascade.size;
    values.scales[i] = render_math::cascade_scale(cascade, cascades.count);
  }
  return values;
}

}  // namespace

void Renderer::Impl::draw_shadow_map(const Camera3D& camera, const Environment& env) {
  const Vec3 to_sun = normalize(Vec3{-sun_travel.x, -sun_travel.y, -sun_travel.z});
  const float strength = render_math::sun_shadow_strength(to_sun, env.shadows);
  if (strength > 0.0f) draw_sun_depth(camera, to_sun, env);
  send(shader, lit_uniforms.shadow_strength, strength);
  send(shader, lit_uniforms.shadow_texel, kShadowTexel);
  bind_for_the_frame(shader, lit_uniforms.shadow_map, kShadowTextureSlot, shadow.depth.id);
}

// The scene's depth as the sun sees it, and the matrices that lead the lit shader into that view.
void Renderer::Impl::draw_sun_depth(const Camera3D& camera, Vec3 to_sun, const Environment& env) {
  const Cascades cascades = render_math::fit_cascades(cascade_camera(camera, view_tan_half_fov, view_aspect), to_sun,
                                                      env.shadow_cascades, env.shadow_distance);
  send(depth_shader, depth_uniforms.time, now());
  send(depth_shader, depth_uniforms.view_position, camera.position);
  const double game_near = rlGetCullDistanceNear();
  const double game_far = rlGetCullDistanceFar();
  rlSetClipPlanes(kShadowNearPlane, shadow_far_plane(cascades));
  draw_cascades(cascades, to_vec3(camera.position));
  rlSetClipPlanes(game_near, game_far);
  send_cascades(cascades, sun_size_of(env));
}

void Renderer::Impl::draw_cascades(const Cascades& cascades, Vec3 viewer) {
  BeginTextureMode(shadow);
  ClearBackground(kWhite);
  for (int i = 0; i < cascades.count; ++i) {
    const Cascade& cascade = cascades.cascades[static_cast<std::size_t>(i)];
    rlViewport(pixels_of(cascade.corner.x), pixels_of(cascade.corner.y), pixels_of(cascade.size),
               pixels_of(cascade.size));
    light_matrices[static_cast<std::size_t>(i)] = draw_depth_from(sun_camera(cascade), viewer);
  }
  EndTextureMode();
}

Matrix Renderer::Impl::draw_depth_from(const Camera3D& sun_view, Vec3 viewer) {
  BeginMode3D(sun_view);
  const Matrix light_matrix = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
  draw_objects(true, viewer);
  EndMode3D();
  return light_matrix;
}

void Renderer::Impl::send_cascades(const Cascades& cascades, float sun_size) {
  const CascadeUniforms values = cascade_uniforms(cascades);
  const std::array<float, kMaxShadowCascades * kFloatsPerTile>& tiles = values.tiles;
  const std::array<float, kMaxShadowCascades>& scales = values.scales;
  rlEnableShader(shader.id);
  rlSetUniformMatrices(lit_uniforms.light_matrix, light_matrices.data(), cascades.count);
  rlDisableShader();
  SetShaderValueV(shader, lit_uniforms.shadow_tiles, tiles.data(), SHADER_UNIFORM_VEC4, cascades.count);
  SetShaderValueV(shader, lit_uniforms.shadow_scales, scales.data(), SHADER_UNIFORM_FLOAT, cascades.count);
  send(shader, lit_uniforms.shadow_cascades, cascades.count);
  send(shader, lit_uniforms.sun_size, sun_size);
  send(shader, shaders::kShadowDepthRange, static_cast<float>(shadow_far_plane(cascades) - kShadowNearPlane));
}

}  // namespace redgem
