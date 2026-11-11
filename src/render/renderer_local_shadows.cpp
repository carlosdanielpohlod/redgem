// The shadows of point and spot lights (local_shadows.hpp): each light the frame picked draws the
// scene's depth into its tiles of one atlas, which the lit shader looks up. A spot draws one view
// down its cone; a point draws the six faces of a cube around it.

#include "render/renderer_impl.hpp"

#include <array>
#include <cmath>

#include "render/shaders/uniforms.hpp"

namespace redgem {
namespace {

using namespace render_detail;
using render_math::LocalShadow;
using render_math::ShadowFace;
using render_math::to_raylib;

constexpr int kTilePixels = render_math::kLocalShadowAtlasSize / render_math::kLocalTilesAcross;
constexpr int kFloatsPerShadow = 4;  // its first tile, its spread, its near and far planes
constexpr float kCubeSpread = 1.0f;  // a cube face sees as far off its axis as along it
constexpr float kNoSpread = 0.0f;    // tells the lit shader a shadow is a point light's cube

int atlas_pixels(float uv) {
  return static_cast<int>(std::lround(uv * static_cast<float>(render_math::kLocalShadowAtlasSize)));
}

// A light's view of one tile: from where it is, along `face`, as wide as `spread` (the tangent of
// half its field of view).
Camera3D face_camera(Vec3 position, const ShadowFace& face, float spread) {
  Camera3D view{};
  view.projection = CAMERA_PERSPECTIVE;
  view.position = to_raylib(position);
  view.target = to_raylib(position + face.forward);
  view.up = to_raylib(face.up);
  view.fovy = radians_to_degrees(kTwo * std::atan(spread));
  return view;
}

float spread_of(const PointLight& light) { return light.spot ? render_math::spot_spread(light) : kNoSpread; }

// Each shadow as the lit shader's localShadows: first tile, spread, near and far planes.
std::array<float, render_math::kMaxLocalShadows * kFloatsPerShadow> shadow_views(
    const std::vector<LocalShadow>& shadows, const std::vector<PointLight>& lights) {
  std::array<float, render_math::kMaxLocalShadows * kFloatsPerShadow> values{};
  for (std::size_t i = 0; i < shadows.size(); ++i) {
    const PointLight& light = lights[shadows[i].light];
    float* view = &values[i * kFloatsPerShadow];
    view[kXOffset] = static_cast<float>(shadows[i].first_tile);
    view[kYOffset] = spread_of(light);
    view[kZOffset] = render_math::kLocalShadowNear;
    view[kWOffset] = light.range;
  }
  return values;
}

}  // namespace

void Renderer::Impl::draw_local_shadows(Vec3 viewer) {
  if (!local_shadows_wanted()) {
    send(shader, lit_uniforms.local_shadow_count, 0);
    return;
  }
  send(depth_shader, depth_uniforms.time, now());
  send(depth_shader, depth_uniforms.view_position, to_raylib(viewer));
  const double game_near = rlGetCullDistanceNear();
  const double game_far = rlGetCullDistanceFar();
  BeginTextureMode(local_shadow);
  ClearBackground(kWhite);
  for (const LocalShadow& light_shadow : local_shadows) draw_light_shadow(light_shadow, viewer);
  EndTextureMode();
  rlSetClipPlanes(game_near, game_far);
  send_local_shadows();
}

// Whether the frame draws shadows of point and spot lights: it picked some, and the card holds
// their atlas.
bool Renderer::Impl::local_shadows_wanted() { return !local_shadows.empty() && ensure_local_shadow_atlas(); }

void Renderer::Impl::draw_light_shadow(const LocalShadow& light_shadow, Vec3 viewer) {
  const PointLight& light = frame_lights[light_shadow.light];
  rlSetClipPlanes(render_math::kLocalShadowNear, light.range);
  if (light.spot) {
    const Camera3D view = face_camera(light.position, render_math::spot_face(light), render_math::spot_spread(light));
    draw_shadow_tile(view, light_shadow.first_tile, viewer);
    return;
  }
  for (int face = 0; face < light_shadow.tiles; ++face) {
    const Camera3D view = face_camera(light.position, render_math::cube_face(face), kCubeSpread);
    draw_shadow_tile(view, light_shadow.first_tile + face, viewer);
  }
}

void Renderer::Impl::draw_shadow_tile(const Camera3D& view, int tile, Vec3 viewer) {
  const render_math::ShadowTile place = render_math::shadow_tile(tile);
  rlViewport(atlas_pixels(place.x), atlas_pixels(place.y), kTilePixels, kTilePixels);
  BeginMode3D(view);
  draw_objects(true, viewer);
  EndMode3D();
}

void Renderer::Impl::send_local_shadows() {
  const auto views = shadow_views(local_shadows, frame_lights);
  const int count = static_cast<int>(local_shadows.size());
  SetShaderValueV(shader, lit_uniforms.local_shadows, views.data(), SHADER_UNIFORM_VEC4, count);
  send(shader, lit_uniforms.local_shadow_count, count);
  bind_for_the_frame(shader, lit_uniforms.local_shadow_map, kLocalShadowTextureSlot, local_shadow.depth.id);
}

}  // namespace redgem
