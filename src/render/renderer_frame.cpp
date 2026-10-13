// A frame: the camera it is seen from, what is prepared before the picture, and the lit picture.

#include "render/renderer_impl.hpp"

#include <cmath>
#include <optional>

#include "render/render_config.hpp"

namespace redgem {
namespace {

using namespace render_detail;
using render_math::to_raylib;
using render_math::to_vec3;

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

}  // namespace

Camera3D Renderer::Impl::camera_for(const Scene& scene) const {
  const ObjectId id = scene.main_camera();
  if (id == kNoObject) return default_camera();
  const Transform world = scene.world_transform(id);
  Camera3D camera{};
  camera.projection = CAMERA_PERSPECTIVE;
  camera.position = to_raylib(world.position);
  camera.target = to_raylib(world.position + world.forward());
  camera.up = to_raylib(world.up());
  const std::optional<CameraDesc>& lens = scene.at(id).camera;
  const float fov = lens ? lens->fov_y_radians : kUnsetFov;
  camera.fovy = fov > 0.0f ? radians_to_degrees(fov) : config::kDefaultFovYDegrees;
  return camera;
}

void Renderer::Impl::prepare_frame(const Scene& scene, const Camera3D& camera, const Environment& env, float aspect) {
  // Where every object is, worked out once: each of the frame's passes (shadows, reflections, the
  // view) would otherwise walk every object's chain of parents again.
  render_math::world_transforms(scene, worlds);
  apply_lights(scene, env, to_vec3(camera.position));
  view_tan_half_fov = std::tan(camera.fovy * DEG2RAD * kHalf);
  view_aspect = aspect;
  draw_shadow_map(scene, camera, env);
  capture_reflection_face(scene, camera);
  send(shader, shader.locs[SHADER_LOC_VECTOR_VIEW], camera.position);
}

void Renderer::Impl::draw_lit_scene(const Scene& scene, const Camera3D& camera, const ParticleSystem* particles) {
  BeginTextureMode(hdr);
  ClearBackground(kBlack);
  BeginMode3D(camera);
  draw_sky(camera);
  draw_objects(scene, false, to_vec3(camera.position));
  if (particles != nullptr) draw_particles(*particles, camera);
  EndMode3D();
  EndTextureMode();
}

void Renderer::Impl::bind_for_the_frame(int sampler_location, int slot, unsigned int texture) {
  rlEnableShader(shader.id);
  rlActiveTextureSlot(slot);
  rlEnableTexture(texture);
  rlSetUniform(sampler_location, &slot, SHADER_UNIFORM_INT, 1);
  rlActiveTextureSlot(0);
}

}  // namespace redgem
