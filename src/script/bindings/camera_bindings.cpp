// Camera natives: the camera the game is seen through, and choosing another. What it does on its
// own (follow, orbit, shake, cut) is built on these in ruby/engine/camera*.rb.

#include <mruby.h>
#include <mruby/array.h>

#include <algorithm>
#include <utility>

#include "scene/camera_ray.hpp"
#include "scene/scene_config.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using namespace binding;

constexpr mrb_int kPairParts = 2;  // a ray is [origin, direction], a point of the screen [x, y]
constexpr double kMinFovDegrees = 10.0;
constexpr double kMaxFovDegrees = 150.0;
constexpr double kNoBlur = 0.0;
constexpr double kFullBlur = 1.0;
constexpr mrb_int kNoLight = 0;        // a channel of black, and a cover that hides nothing
constexpr mrb_int kFullChannel = 255;  // a channel at its most: a cover that hides everything

// The main camera's own description, or nullptr when the scene has no camera.
CameraDesc* main_camera(mrb_state* mrb) {
  const ObjectId id = scene(mrb).main_camera();
  if (id == kNoObject) return nullptr;
  std::optional<CameraDesc>& camera = scene(mrb).at(id).camera;
  return camera ? &*camera : nullptr;
}

mrb_value camera_main(mrb_state* mrb, mrb_value) { return object_or_nil(mrb, scene(mrb).main_camera()); }

// An object the game can be seen through: one still there and staying, not a template, holding a
// camera. An error starting with `asker` otherwise.
ObjectId viewable_camera(mrb_state* mrb, mrb_value object, const char* asker) {
  const ObjectId id = placeable_id(mrb, object, asker);
  if (!scene(mrb).at(id).camera) mrb_raisef(mrb, E_ARGUMENT_ERROR, "%s %v holds no camera", asker, object);
  return id;
}

// Camera.__choose(object or nil, asker): the camera the game is seen through from now on; nil
// goes back to the scene's first camera.
mrb_value camera_choose(mrb_state* mrb, mrb_value) {
  mrb_value object;
  const char* asker = nullptr;
  mrb_get_args(mrb, "oz", &object, &asker);
  scene(mrb).choose_main_camera(mrb_nil_p(object) ? kNoObject : viewable_camera(mrb, object, asker));
  return object;
}

// Camera.__check(object, asker): the object, when the game can be seen through it.
mrb_value camera_check(mrb_state* mrb, mrb_value) {
  mrb_value object;
  const char* asker = nullptr;
  mrb_get_args(mrb, "oz", &object, &asker);
  viewable_camera(mrb, object, asker);
  return object;
}

// Camera.__darken(alpha): black over the whole picture, 0 to 255 opaque, under everything the UI
// draws this frame, before it or after (the fade of a cut: Camera::Cut).
mrb_value camera_darken(mrb_state* mrb, mrb_value) {
  mrb_int alpha = 0;
  mrb_get_args(mrb, "i", &alpha);
  ScriptContext& ctx = context(mrb);
  HudItem cover;
  cover.kind = HudItem::Kind::Rect;
  cover.width = ctx.screen_width;
  cover.height = ctx.screen_height;
  cover.color = color_from(kNoLight, kNoLight, kNoLight, std::clamp<mrb_int>(alpha, kNoLight, kFullChannel));
  ctx.hud.insert(ctx.hud.begin(), std::move(cover));
  return mrb_nil_value();
}

// The screen scripts draw on, and the world's picture on it as the presenter draws it (at a render
// scale, into a recording) and shows it stretched over the screen.
ScreenFrame screen_frame(const ScriptContext& ctx) {
  return {ctx.screen_width, ctx.screen_height, ctx.picture_width, ctx.picture_height};
}

// (Game cameras are drawn in perspective.)
Lens lens_of(const CameraDesc& camera) { return {camera.fov_y_radians, 0.0f}; }

// Where the main camera sees from: its place, and its shake on it (as the renderer draws it).
Transform main_camera_view(mrb_state* mrb) {
  const ObjectId id = scene(mrb).main_camera();
  return viewed_from(scene(mrb).world_transform(id), scene(mrb).at(id).camera_shake);
}

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
  const float scale = ui_scale_of(context(mrb));  // (x and y are in the interface's units)
  const ScreenPoint point{static_cast<float>(finite(mrb, x)) * scale, static_cast<float>(finite(mrb, y)) * scale};
  const CameraDesc* camera = main_camera(mrb);
  const ScreenFrame frame = screen_frame(context(mrb));
  const bool can_look = camera != nullptr && has_area(frame);
  if (!can_look) return mrb_nil_value();
  const CameraRay ray = screen_ray(main_camera_view(mrb), lens_of(*camera), frame, point);
  return pair(mrb, vec3_value(mrb, ray.origin), vec3_value(mrb, ray.direction));
}

// A place in the world given as a GameObject (where it is) or as a Vector3.
Vec3 point_of(mrb_state* mrb, mrb_value target) {
  if (!is_game_object(mrb, target)) return to_vec3(mrb, target);
  return scene(mrb).world_transform(alive_id(mrb, target)).position;
}

// Where the main camera shows a point of the world on the screen, [x, y] in the units the
// interface is drawn in (UI.scale pixels each); nil behind the camera (or nearer than its near plane).
mrb_value camera_world_to_screen(mrb_state* mrb, mrb_value) {
  const Vec3 point = point_of(mrb, single_arg(mrb));
  const CameraDesc* camera = main_camera(mrb);
  if (camera == nullptr) return mrb_nil_value();
  const auto shown = world_to_screen(main_camera_view(mrb), lens_of(*camera), screen_frame(context(mrb)), point);
  if (!shown) return mrb_nil_value();
  const float scale = ui_scale_of(context(mrb));
  return pair(mrb, mrb_float_value(mrb, shown->x / scale), mrb_float_value(mrb, shown->y / scale));
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

// Shakes a camera: what it shows moves by `move` meters (right, up, back) and turns by `turn`
// degrees (pitch, yaw, roll) in its own axes, until the next shake; the object itself stays put
// (see GameObject::camera_shake). Camera::Rig gives the offsets.
mrb_value camera_shake_by(mrb_state* mrb, mrb_value) {
  mrb_value object = mrb_nil_value(), move = mrb_nil_value(), turn = mrb_nil_value();
  mrb_get_args(mrb, "ooo", &object, &move, &turn);
  GameObject& camera = alive_object(mrb, object);
  if (!camera.camera) mrb_raise(mrb, E_ARGUMENT_ERROR, "only a camera shakes");
  camera.camera_shake.position = to_vec3(mrb, move);
  camera.camera_shake.rotation = quat_from_euler_degrees(to_vec3(mrb, turn));
  return object;
}

}  // namespace

void install_camera_bindings(mrb_state* mrb) {
  RClass* camera_module = mrb_define_module(mrb, "Camera");
  mrb_define_module_function(mrb, camera_module, "main", camera_main, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, camera_module, "__choose", camera_choose, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, camera_module, "__check", camera_check, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, camera_module, "__darken", camera_darken, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, camera_module, "screen_ray", camera_screen_ray, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, camera_module, "world_to_screen", camera_world_to_screen, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, camera_module, "fov", camera_fov, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, camera_module, "fov=", camera_set_fov, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, camera_module, "motion_blur", camera_motion_blur, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, camera_module, "motion_blur=", camera_set_motion_blur, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, camera_module, "__shake_by", camera_shake_by, MRB_ARGS_REQ(3));
}

}  // namespace redgem
