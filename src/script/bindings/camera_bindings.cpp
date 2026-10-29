// Camera natives: the scene's main camera. What it does on its own (follow, orbit, shake) is built
// on these in ruby/engine/camera.rb.

#include <mruby.h>
#include <mruby/array.h>

#include <algorithm>

#include "render/display_settings.hpp"
#include "scene/camera_ray.hpp"
#include "scene/scene_config.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using namespace binding;

constexpr mrb_int kPairParts = 2;  // a ray is [origin, direction], a point of the screen [x, y]
// What a pose is made of, in its order: a place, then a turn (a quaternion).
enum PosePart : mrb_int { kPlaceX, kPlaceY, kPlaceZ, kTurnX, kTurnY, kTurnZ, kTurnW, kPoseParts };
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

// The screen scripts draw on, and the world's picture on it: drawn at the player's render scale,
// then stretched over the screen (see WindowPresenter).
ScreenFrame screen_frame(const ScriptContext& ctx) {
  const SurfaceSize picture = scaled_size({ctx.screen_width, ctx.screen_height}, ctx.display.render_scale);
  return {ctx.screen_width, ctx.screen_height, picture.width, picture.height};
}

// (Game cameras are drawn in perspective.)
Lens lens_of(const CameraDesc& camera) { return {camera.fov_y_radians, 0.0f}; }

Transform main_camera_world(mrb_state* mrb) { return scene(mrb).world_transform(scene(mrb).main_camera()); }

mrb_value pair(mrb_state* mrb, mrb_value first, mrb_value second) {
  mrb_value both = mrb_ary_new_capa(mrb, kPairParts);
  mrb_ary_push(mrb, both, first);
  mrb_ary_push(mrb, both, second);
  return both;
}

// The ray that leaves the main camera through a point of the screen: [origin, direction].
mrb_value camera_screen_ray(mrb_state* mrb, mrb_value) {
  mrb_float x = 0.0, y = 0.0;
  mrb_get_args(mrb, "ff", &x, &y);
  const ScreenPoint point{static_cast<float>(finite(mrb, x)), static_cast<float>(finite(mrb, y))};
  const CameraDesc* camera = main_camera(mrb);
  const ScreenFrame frame = screen_frame(context(mrb));
  const bool can_look = camera != nullptr && has_area(frame);
  if (!can_look) return mrb_nil_value();
  const CameraRay ray = screen_ray(main_camera_world(mrb), lens_of(*camera), frame, point);
  return pair(mrb, vec3_value(mrb, ray.origin), vec3_value(mrb, ray.direction));
}

// A place in the world given as a GameObject (where it is) or as a Vector3.
Vec3 point_of(mrb_state* mrb, mrb_value target) {
  if (!is_game_object(mrb, target)) return to_vec3(mrb, target);
  return scene(mrb).world_transform(alive_id(mrb, target)).position;
}

// Where the main camera shows a point of the world on the screen, [x, y] in the pixels the
// interface is drawn in; nil behind the camera (or nearer than its near plane).
mrb_value camera_world_to_screen(mrb_state* mrb, mrb_value) {
  const Vec3 point = point_of(mrb, single_arg(mrb));
  const CameraDesc* camera = main_camera(mrb);
  if (camera == nullptr) return mrb_nil_value();
  const auto shown = world_to_screen(main_camera_world(mrb), lens_of(*camera), screen_frame(context(mrb)), point);
  if (!shown) return mrb_nil_value();
  return pair(mrb, mrb_float_value(mrb, shown->x), mrb_float_value(mrb, shown->y));
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

// An object's own place and turn, as kept relative to its parent: [x, y, z, qx, qy, qz, qw]. The
// camera helpers keep the pose a shake started from, to put it back exactly: through world
// positions and Euler angles it would come back a rounding off, and a moving parent would carry it.
mrb_value camera_pose(mrb_state* mrb, mrb_value) {
  const Transform& local = alive_object(mrb, single_arg(mrb)).local;
  mrb_value pose = mrb_ary_new_capa(mrb, kPoseParts);
  for (const float part : {local.position.x, local.position.y, local.position.z, local.rotation.x, local.rotation.y,
                           local.rotation.z, local.rotation.w}) {
    mrb_ary_push(mrb, pose, mrb_float_value(mrb, part));
  }
  return pose;
}

float pose_part(mrb_state* mrb, mrb_value pose, mrb_int index) {
  return static_cast<float>(finite(mrb, mrb_as_float(mrb, mrb_ary_ref(mrb, pose, index))));
}

void notify_moved(mrb_state* mrb, ObjectId id) {
  const ScriptContext& ctx = context(mrb);
  if (!ctx.on_transform_written) return;
  ctx.on_transform_written(id, TransformWrite::Position);
  ctx.on_transform_written(id, TransformWrite::Rotation);
}

// Puts an object back in a pose camera_pose gave.
mrb_value camera_set_pose(mrb_state* mrb, mrb_value) {
  mrb_value object = mrb_nil_value(), pose = mrb_nil_value();
  mrb_get_args(mrb, "oA", &object, &pose);
  if (RARRAY_LEN(pose) != kPoseParts) mrb_raise(mrb, E_ARGUMENT_ERROR, "a pose is 7 numbers");
  GameObject& moved = alive_object(mrb, object);
  moved.local.position = {pose_part(mrb, pose, kPlaceX), pose_part(mrb, pose, kPlaceY), pose_part(mrb, pose, kPlaceZ)};
  moved.local.rotation = normalize(Quat{pose_part(mrb, pose, kTurnX), pose_part(mrb, pose, kTurnY),
                                        pose_part(mrb, pose, kTurnZ), pose_part(mrb, pose, kTurnW)});
  notify_moved(mrb, moved.id);
  return object;
}

}  // namespace

void install_camera_bindings(mrb_state* mrb) {
  RClass* camera_module = mrb_define_module(mrb, "Camera");
  mrb_define_module_function(mrb, camera_module, "main", camera_main, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, camera_module, "screen_ray", camera_screen_ray, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, camera_module, "world_to_screen", camera_world_to_screen, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, camera_module, "fov", camera_fov, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, camera_module, "fov=", camera_set_fov, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, camera_module, "motion_blur", camera_motion_blur, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, camera_module, "motion_blur=", camera_set_motion_blur, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, camera_module, "__pose", camera_pose, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, camera_module, "__set_pose", camera_set_pose, MRB_ARGS_REQ(2));
}

}  // namespace redgem
