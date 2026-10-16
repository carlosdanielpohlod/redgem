// Camera natives: the scene's main camera.

#include <mruby.h>
#include <mruby/array.h>

#include <algorithm>

#include "scene/camera_ray.hpp"
#include "scene/scene_config.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using namespace binding;

constexpr mrb_int kRayParts = 2;  // a ray is [origin, direction]
constexpr double kMinFovDegrees = 10.0;
constexpr double kMaxFovDegrees = 150.0;
constexpr double kNoBlur = 0.0;
constexpr double kFullBlur = 1.0;

// The main camera's own description, or nullptr when the scene has no camera.
CameraDesc* main_camera(mrb_state* mrb) {
  const ObjectId id = scene(mrb).main_camera();
  if (id == kNoObject) return nullptr;
  std::optional<CameraDesc>& camera = scene(mrb).at(id).camera;
  return camera ? &*camera : nullptr;
}

mrb_value camera_main(mrb_state* mrb, mrb_value) { return object_or_nil(mrb, scene(mrb).main_camera()); }

// Whether there is a screen for a ray to leave through.
bool has_a_screen(const ScriptContext& ctx) { return ctx.screen_width > 0 && ctx.screen_height > 0; }

// The ray that leaves the main camera through a point of the screen: [origin, direction].
mrb_value camera_screen_ray(mrb_state* mrb, mrb_value) {
  mrb_float x = 0.0, y = 0.0;
  mrb_get_args(mrb, "ff", &x, &y);
  x = finite(mrb, x);
  y = finite(mrb, y);
  const ScriptContext& ctx = context(mrb);
  const CameraDesc* camera = main_camera(mrb);
  const bool can_look = camera != nullptr && has_a_screen(ctx);
  if (!can_look) return mrb_nil_value();
  const Transform world = scene(mrb).world_transform(scene(mrb).main_camera());
  const ScreenPoint point{static_cast<float>(x), static_cast<float>(y), ctx.screen_width, ctx.screen_height};
  mrb_value ray = mrb_ary_new_capa(mrb, kRayParts);
  mrb_ary_push(mrb, ray, vec3_value(mrb, world.position));
  mrb_ary_push(mrb, ray, vec3_value(mrb, screen_ray_direction(world, camera->fov_y_radians, point)));
  return ray;
}

// The main camera's field of view, top to bottom, in degrees.
mrb_value camera_fov(mrb_state* mrb, mrb_value) {
  const CameraDesc* camera = main_camera(mrb);
  if (camera == nullptr) return mrb_nil_value();
  const float fov = camera->fov_y_radians;
  return mrb_float_value(mrb, fov > 0.0f ? radians_to_degrees(fov) : config::kDefaultFovYDegrees);
}

mrb_value camera_set_fov(mrb_state* mrb, mrb_value) {
  const mrb_float degrees = from_to(mrb, float_arg(mrb), kMinFovDegrees, kMaxFovDegrees, "Camera.fov");
  CameraDesc* camera = main_camera(mrb);
  if (camera != nullptr) camera->fov_y_radians = degrees_to_radians(static_cast<float>(degrees));
  return mrb_float_value(mrb, degrees);
}

mrb_value camera_motion_blur(mrb_state* mrb, mrb_value) { return mrb_float_value(mrb, context(mrb).motion_blur); }

mrb_value camera_set_motion_blur(mrb_state* mrb, mrb_value) {
  const mrb_float amount = from_to(mrb, float_arg(mrb), kNoBlur, kFullBlur, "Camera.motion_blur");
  context(mrb).motion_blur = static_cast<float>(amount);
  return mrb_float_value(mrb, amount);
}

}  // namespace

void install_camera_bindings(mrb_state* mrb) {
  RClass* camera_module = mrb_define_module(mrb, "Camera");
  mrb_define_module_function(mrb, camera_module, "main", camera_main, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, camera_module, "screen_ray", camera_screen_ray, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, camera_module, "fov", camera_fov, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, camera_module, "fov=", camera_set_fov, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, camera_module, "motion_blur", camera_motion_blur, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, camera_module, "motion_blur=", camera_set_motion_blur, MRB_ARGS_REQ(1));
}

}  // namespace redgem
