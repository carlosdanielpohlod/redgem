// The sun's shadow map: the scene's depth as the sun sees it, for the lit shader to look up.

#include "render/renderer_impl.hpp"

#include <raymath.h>

#include "render/shaders/uniforms.hpp"

namespace redgem {
namespace {

using namespace render_detail;
using render_math::kShadowExtent;
using render_math::kShadowMapSize;
using render_math::kShadowSunDistance;
using render_math::to_vec3;

// The sun's view only needs the depth range that can hold casters and receivers; a tight range
// keeps depth precise (and the shadow bias small).
constexpr double kShadowNearPlane = 1.0;                                   // meters
constexpr double kShadowFarPlane = kTwo * kShadowSunDistance;              // meters
constexpr float kShadowTexel = 1.0f / static_cast<float>(kShadowMapSize);  // in texture coordinates

// The sun's point of view: straight (no perspective) and wide enough for everything shadowed.
Camera3D sun_camera(const render_math::SunView& fitted) {
  Camera3D sun_view{};
  sun_view.projection = CAMERA_ORTHOGRAPHIC;
  sun_view.fovy = kShadowExtent;
  sun_view.target = fitted.target;
  sun_view.position = fitted.position;
  sun_view.up = fitted.up;
  return sun_view;
}

}  // namespace

void Renderer::Impl::draw_shadow_map(const Scene& scene, const Camera3D& camera, const Environment& env) {
  const Vec3 to_sun = normalize(Vec3{-sun_travel.x, -sun_travel.y, -sun_travel.z});
  const float strength = render_math::sun_shadow_strength(to_sun, env.shadows);
  if (strength > 0.0f) draw_sun_depth(scene, camera, to_sun);
  send(shader, lit_uniforms.shadow_strength, strength);
  send(shader, lit_uniforms.shadow_texel, kShadowTexel);
  bind_for_the_frame(shader, lit_uniforms.shadow_map, kShadowTextureSlot, shadow.depth.id);
}

// The scene's depth as the sun sees it, and the matrix that leads the lit shader into that view.
void Renderer::Impl::draw_sun_depth(const Scene& scene, const Camera3D& camera, Vec3 to_sun) {
  const Camera3D sun_view = sun_camera(render_math::fit_sun_view(camera.position, camera.target, to_sun));
  send(depth_shader, depth_uniforms.time, static_cast<float>(GetTime()));
  const double game_near = rlGetCullDistanceNear();
  const double game_far = rlGetCullDistanceFar();
  rlSetClipPlanes(kShadowNearPlane, kShadowFarPlane);
  const Matrix light_matrix = draw_depth_from(sun_view, scene, to_vec3(camera.position));
  rlSetClipPlanes(game_near, game_far);
  SetShaderValueMatrix(shader, lit_uniforms.light_matrix, light_matrix);
  send(shader, shaders::kShadowDepthRange, static_cast<float>(kShadowFarPlane - kShadowNearPlane));
}

Matrix Renderer::Impl::draw_depth_from(const Camera3D& sun_view, const Scene& scene, Vec3 viewer) {
  BeginTextureMode(shadow);
  ClearBackground(kWhite);
  BeginMode3D(sun_view);
  const Matrix light_matrix = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
  draw_objects(scene, true, viewer);
  EndMode3D();
  EndTextureMode();
  return light_matrix;
}

}  // namespace redgem
