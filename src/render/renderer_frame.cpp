// A frame: the camera it is seen from, what is prepared before the picture, and the lit picture.

#include "render/renderer_impl.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

#include "render/render_config.hpp"
#include "scene/camera_ray.hpp"

namespace redgem {
namespace {

using namespace render_detail;
using render_math::to_raylib;
using render_math::to_vec3;

bool water_surface(const MaterialDesc& material) { return material.water.enabled; }

bool coated(const MaterialDesc& material) { return material.clearcoat.factor > 0.0f; }

constexpr float kUnsetFov = 0.0f;  // radians: a camera that leaves its field of view to the engine

// Where scenes without a camera are seen from: above and behind the origin, looking at it.
Camera3D default_camera() {
  Camera3D camera{};
  camera.projection = CAMERA_PERSPECTIVE;
  camera.position = {0.0f, config::kDefaultCameraDistance * kHalf, config::kDefaultCameraDistance};
  camera.target = {0.0f, 0.0f, 0.0f};
  camera.up = render_math::kUpAxis;
  camera.fovy = config::kDefaultFovYDegrees;
  return camera;
}

// Whether a frame has particles: a system without any lays nothing out (what is laid out then is
// an earlier frame's).
bool has_particles(const ParticleSystem* system) { return system != nullptr && !system->particles().empty(); }

// The picture of several samples a pixel as raylib draws into a target: its framebuffer and size.
RenderTexture2D target_of(const Multisampled& samples) {
  RenderTexture2D target{};
  target.id = samples.framebuffer;
  target.texture.width = samples.width;
  target.texture.height = samples.height;
  return target;
}

}  // namespace

Camera3D Renderer::Impl::camera_for(const Scene& scene) const {
  const ObjectId id = scene.main_camera();
  if (id == kNoObject) return default_camera();
  const std::optional<CameraDesc>& lens = scene.at(id).camera;
  // (A shake moves what the camera shows, never the camera.)
  const Transform world = viewed_from(scene.world_transform(id), scene.at(id).camera_shake);
  Camera3D camera{};
  camera.projection = CAMERA_PERSPECTIVE;
  camera.position = to_raylib(world.position);
  camera.target = to_raylib(world.position + world.forward());
  camera.up = to_raylib(world.up());
  const float fov = lens ? lens->fov_y_radians : kUnsetFov;
  camera.fovy = fov > 0.0f ? radians_to_degrees(fov) : config::kDefaultFovYDegrees;
  return camera;
}

void Renderer::Impl::prepare_frame(const Scene& scene, const Camera3D& camera, const Environment& env, float aspect,
                                   const ParticleSystem* particles) {
  use_environment(env, camera);
  // Where every object is, worked out once: each of the frame's passes (shadows, reflections, the
  // view) would otherwise walk every object's chain of parents again. So are its bones.
  render_math::world_transforms(scene, worlds);
  slots.used = 0;  // the frame's passes start over in the buffer of what they see
  write_frame_texels(scene);
  send_frame_texels();
  apply_lights(scene, env, to_vec3(camera.position));
  view_tan_half_fov = std::tan(camera.fovy * DEG2RAD * kHalf);
  view_aspect = aspect;
  draw_shadow_map(camera, env, particles);
  capture_reflection_face(camera);
  send(shader, shader.locs[SHADER_LOC_VECTOR_VIEW], camera.position);
}

void Renderer::Impl::use_environment(const Environment& env, const Camera3D& camera) {
  use_sky_picture(env);
  const bool clearcoat = std::any_of(material_factors.begin(), material_factors.end(), coated);
  const bool water = std::any_of(material_factors.begin(), material_factors.end(), water_surface);
  use_lit_features({loaded(sky.picture), env.shadow_softness > 0.0f, clearcoat, water});
  if (water) ensure_water_target();
  send(shader, lit_uniforms.water_ready, render_math::kOff);
  begin_temporal(env, camera);
}

void Renderer::Impl::draw_lit_scene(const Camera3D& camera, const ParticleSystem* particles) {
  glow_gathered = false;  // until this frame's particles gather some (a frame may have none)
  const bool sampled = scene_samples.framebuffer != 0;
  BeginTextureMode(sampled ? target_of(scene_samples) : hdr);
  ClearBackground(kBlack);
  BeginMode3D(camera);
  if (temporal.on) jitter_view();
  draw_sky(camera);
  draw_surfaces(to_vec3(camera.position), sampled);
  draw_scene_particles(particles, camera);
  EndMode3D();
  if (glow_gathered) add_gathered_glow();
  EndTextureMode();
  if (sampled) resolve_samples();
}

void Renderer::Impl::draw_surfaces(Vec3 viewer, bool sampled) {
  covering = sampled;
  const std::size_t first = draw_objects(false, viewer);
  covering = false;
  if (temporal.on) draw_motion(first, viewer);
  if (lit_features.water) capture_water_background();
  draw_see_through(first, viewer);
  if (motion_needed() && !see_through.empty()) mark_see_through_cover(viewer);
}

void Renderer::Impl::draw_scene_particles(const ParticleSystem* particles, const Camera3D& camera) {
  if (particles == nullptr) return;
  draw_particles(*particles, camera);
  if (motion_needed() && has_particles(particles)) mark_particle_cover();
}

void Renderer::Impl::bind_for_the_frame(const Shader& program, int sampler_location, int slot, unsigned int texture) {
  rlEnableShader(program.id);
  rlActiveTextureSlot(slot);
  rlEnableTexture(texture);
  rlSetUniform(sampler_location, &slot, SHADER_UNIFORM_INT, 1);
  rlActiveTextureSlot(0);
}

void Renderer::Impl::bind_cube_for_the_frame(const Shader& program, int sampler_location, int slot,
                                             unsigned int texture) {
  rlEnableShader(program.id);
  rlActiveTextureSlot(slot);
  rlEnableTextureCubemap(texture);
  rlSetUniform(sampler_location, &slot, SHADER_UNIFORM_INT, 1);
  rlActiveTextureSlot(0);
}

}  // namespace redgem
