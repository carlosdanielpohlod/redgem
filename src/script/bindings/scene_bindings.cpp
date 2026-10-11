// GameObject, Transform, Scene and Camera natives.

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/hash.h>
#include <mruby/string.h>

#include <cmath>

#include "core/config.hpp"
#include "core/log.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"
#include "script/script_host.hpp"

namespace redgem {
namespace {

using namespace binding;

constexpr float kHalf = 0.5f;
constexpr float kFullSpan = 2.0f;  // a screen spans -1..1
constexpr float kHalfSpan = 1.0f;
constexpr double kMinFovDegrees = 10.0;
constexpr double kMaxFovDegrees = 150.0;
constexpr double kFullBlur = 1.0;

void notify_transform_written(mrb_state* mrb, ObjectId id, TransformWrite write) {
  ScriptContext& ctx = context(mrb);
  if (ctx.on_transform_written) ctx.on_transform_written(id, write);
}

mrb_value property_value(mrb_state* mrb, const PropertyValue& value) {
  if (const auto* flag = std::get_if<bool>(&value)) return mrb_bool_value(*flag);
  if (const auto* number = std::get_if<double>(&value)) return mrb_float_value(mrb, *number);
  const auto& text = std::get<std::string>(value);
  return mrb_str_new(mrb, text.data(), static_cast<mrb_int>(text.size()));
}

// --- GameObject ------------------------------------------------------------------

// Name and tag stay readable after destruction (scripts often log what they just destroyed).
mrb_value game_object_name(mrb_state* mrb, mrb_value self) {
  return mrb_str_new_cstr(mrb, object_at(mrb, self).name.c_str());
}

mrb_value game_object_tag(mrb_state* mrb, mrb_value self) {
  return mrb_str_new_cstr(mrb, object_at(mrb, self).tag.c_str());
}

mrb_value game_object_alive(mrb_state* mrb, mrb_value self) {
  const GameObject& object = object_at(mrb, self);
  return mrb_bool_value(!object.destroyed && !object.pending_destroy);
}

mrb_value game_object_destroy(mrb_state* mrb, mrb_value self) {
  scene(mrb).request_destroy(object_at(mrb, self).id);
  return mrb_nil_value();
}

mrb_value game_object_props(mrb_state* mrb, mrb_value self) {
  const GameObject& object = alive_object(mrb, self);
  mrb_value hash = mrb_hash_new(mrb);
  for (const auto& [key, value] : object.props) {
    mrb_hash_set(mrb, hash, mrb_symbol_value(mrb_intern_cstr(mrb, key.c_str())), property_value(mrb, value));
  }
  return hash;
}

mrb_value game_object_visible(mrb_state* mrb, mrb_value self) {
  return mrb_bool_value(alive_object(mrb, self).visible);
}

mrb_value game_object_set_visible(mrb_state* mrb, mrb_value self) {
  mrb_bool visible = false;
  mrb_get_args(mrb, "b", &visible);
  alive_object(mrb, self).visible = visible;
  return mrb_bool_value(visible);
}

mrb_value game_object_animation_offset(mrb_state* mrb, mrb_value self) {
  return mrb_float_value(mrb, alive_object(mrb, self).animation_offset);
}

mrb_value game_object_set_animation_offset(mrb_state* mrb, mrb_value self) {
  mrb_float seconds = 0.0;
  mrb_get_args(mrb, "f", &seconds);
  alive_object(mrb, self).animation_offset = static_cast<float>(seconds);
  return mrb_float_value(mrb, seconds);
}

mrb_value game_object_parent(mrb_state* mrb, mrb_value self) {
  return object_or_nil(mrb, alive_object(mrb, self).parent);
}

mrb_value game_object_children(mrb_state* mrb, mrb_value self) {
  const GameObject& object = alive_object(mrb, self);
  mrb_value children = mrb_ary_new(mrb);
  for (const ObjectId child : object.children) {
    if (scene(mrb).is_alive(child)) mrb_ary_push(mrb, children, object_or_nil(mrb, child));
  }
  return children;
}

mrb_value game_object_has_character(mrb_state* mrb, mrb_value self) {
  const ObjectId id = alive_object(mrb, self).id;
  return mrb_bool_value(context(mrb).physics->body_type(id) == BodyType::Character);
}

mrb_value game_object_has_vehicle(mrb_state* mrb, mrb_value self) {
  const ObjectId id = alive_object(mrb, self).id;
  return mrb_bool_value(context(mrb).physics->body_type(id) == BodyType::Vehicle);
}

mrb_value game_object_has_rigidbody(mrb_state* mrb, mrb_value self) {
  const ObjectId id = alive_object(mrb, self).id;
  const BodyType type = context(mrb).physics->body_type(id);
  return mrb_bool_value(type == BodyType::Dynamic || type == BodyType::Kinematic || type == BodyType::Vehicle);
}

mrb_value game_object_add_component(mrb_state* mrb, mrb_value self) {
  mrb_value klass;
  mrb_get_args(mrb, "o", &klass);
  return script_host(mrb).add_behaviour(alive_object(mrb, self).id, klass);
}

// Loads the game script a missing constant should come from (see ruby/engine/kernel.rb).
mrb_value kernel_require_class(mrb_state* mrb, mrb_value) {
  mrb_value name;
  mrb_get_args(mrb, "o", &name);
  return mrb_bool_value(script_host(mrb).require_class(to_name(mrb, name)));
}

// --- Transform ---------------------------------------------------------------------

Transform world_of(mrb_state* mrb, mrb_value self) { return scene(mrb).world_transform(alive_object(mrb, self).id); }

mrb_value transform_position(mrb_state* mrb, mrb_value self) { return vec3_value(mrb, world_of(mrb, self).position); }

mrb_value transform_set_position(mrb_state* mrb, mrb_value self) {
  mrb_value value;
  mrb_get_args(mrb, "o", &value);
  const ObjectId id = alive_object(mrb, self).id;
  scene(mrb).set_world_position(id, to_vec3(mrb, value));
  notify_transform_written(mrb, id, TransformWrite::Position);
  return value;
}

mrb_value transform_local_position(mrb_state* mrb, mrb_value self) {
  return vec3_value(mrb, alive_object(mrb, self).local.position);
}

mrb_value transform_set_local_position(mrb_state* mrb, mrb_value self) {
  mrb_value value;
  mrb_get_args(mrb, "o", &value);
  GameObject& object = alive_object(mrb, self);
  object.local.position = to_vec3(mrb, value);
  notify_transform_written(mrb, object.id, TransformWrite::Position);
  return value;
}

mrb_value transform_rotation(mrb_state* mrb, mrb_value self) {
  return vec3_value(mrb, quat_to_euler_degrees(world_of(mrb, self).rotation));
}

mrb_value transform_set_rotation(mrb_state* mrb, mrb_value self) {
  mrb_value value;
  mrb_get_args(mrb, "o", &value);
  const ObjectId id = alive_object(mrb, self).id;
  scene(mrb).set_world_rotation(id, quat_from_euler_degrees(to_vec3(mrb, value)));
  notify_transform_written(mrb, id, TransformWrite::Rotation);
  return value;
}

mrb_value transform_rotate(mrb_state* mrb, mrb_value self) {
  mrb_value value;
  mrb_get_args(mrb, "o", &value);
  const ObjectId id = alive_object(mrb, self).id;
  // Rotation in the object's own space.
  const Quat delta = quat_from_euler_degrees(to_vec3(mrb, value));
  scene(mrb).set_world_rotation(id, normalize(scene(mrb).world_transform(id).rotation * delta));
  notify_transform_written(mrb, id, TransformWrite::Rotation);
  return self;
}

mrb_value transform_look_at(mrb_state* mrb, mrb_value self) {
  mrb_value target;
  mrb_get_args(mrb, "o", &target);
  const ObjectId id = alive_object(mrb, self).id;
  Vec3 point;
  if (mrb_obj_is_kind_of(mrb, target, mrb_class_get(mrb, "GameObject"))) {
    point = scene(mrb).world_transform(alive_object(mrb, target).id).position;
  } else {
    point = to_vec3(mrb, target);
  }
  const Vec3 direction = point - scene(mrb).world_transform(id).position;
  scene(mrb).set_world_rotation(id, quat_look_rotation(direction, Vec3::up()));
  notify_transform_written(mrb, id, TransformWrite::Rotation);
  return self;
}

mrb_value transform_scale(mrb_state* mrb, mrb_value self) { return vec3_value(mrb, alive_object(mrb, self).local.scale); }

mrb_value transform_set_scale(mrb_state* mrb, mrb_value self) {
  mrb_value value;
  mrb_get_args(mrb, "o", &value);
  GameObject& object = alive_object(mrb, self);
  object.local.scale = to_vec3(mrb, value);
  if (context(mrb).physics->has_object(object.id)) {
    log::warning("object '" + object.name + "': colliders keep the scale they had when the scene loaded");
  }
  return value;
}

mrb_value transform_forward(mrb_state* mrb, mrb_value self) { return vec3_value(mrb, world_of(mrb, self).forward()); }
mrb_value transform_right(mrb_state* mrb, mrb_value self) { return vec3_value(mrb, world_of(mrb, self).right()); }
mrb_value transform_up(mrb_state* mrb, mrb_value self) { return vec3_value(mrb, world_of(mrb, self).up()); }

// --- Scene / Camera ----------------------------------------------------------------

mrb_value scene_find(mrb_state* mrb, mrb_value) {
  mrb_value name;
  mrb_get_args(mrb, "o", &name);
  return object_or_nil(mrb, scene(mrb).find(to_name(mrb, name)));
}

mrb_value ids_to_array(mrb_state* mrb, const std::vector<ObjectId>& ids) {
  mrb_value out = mrb_ary_new(mrb);
  for (const ObjectId id : ids) {
    if (!scene(mrb).at(id).pending_destroy) mrb_ary_push(mrb, out, object_or_nil(mrb, id));
  }
  return out;
}

mrb_value scene_find_all(mrb_state* mrb, mrb_value) {
  mrb_value tag;
  mrb_get_args(mrb, "o", &tag);
  return ids_to_array(mrb, scene(mrb).find_all_by_tag(to_name(mrb, tag)));
}

mrb_value scene_objects(mrb_state* mrb, mrb_value) { return ids_to_array(mrb, scene(mrb).alive_ids()); }

mrb_value scene_spawn(mrb_state* mrb, mrb_value) {
  mrb_value source;
  mrb_value position = mrb_nil_value();
  mrb_value rotation = mrb_nil_value();
  mrb_get_args(mrb, "o|oo", &source, &position, &rotation);
  ScriptContext& ctx = context(mrb);
  if (!ctx.spawn) mrb_raise(mrb, E_RUNTIME_ERROR, "Scene.spawn only works while the game runs");
  ObjectId id = kNoObject;
  if (mrb_obj_is_kind_of(mrb, source, mrb_class_get(mrb, "GameObject"))) {
    id = alive_object(mrb, source).id;
  } else {
    const std::string name = to_name(mrb, source);
    id = scene(mrb).find(name);
    if (id == kNoObject) mrb_raisef(mrb, E_ARGUMENT_ERROR, "no object named %s to spawn", name.c_str());
  }
  std::optional<Transform> world;
  if (!mrb_nil_p(position) || !mrb_nil_p(rotation)) {
    world = scene(mrb).world_transform(id);
    if (!mrb_nil_p(position)) world->position = to_vec3(mrb, position);
    if (!mrb_nil_p(rotation)) world->rotation = quat_from_euler_degrees(to_vec3(mrb, rotation));
  }
  return object_or_nil(mrb, ctx.spawn(id, world));
}

mrb_value camera_main(mrb_state* mrb, mrb_value) { return object_or_nil(mrb, scene(mrb).main_camera()); }

// The ray that leaves the main camera through a point of the screen: [origin, direction].
mrb_value camera_screen_ray(mrb_state* mrb, mrb_value) {
  mrb_float x = 0.0, y = 0.0;
  mrb_get_args(mrb, "ff", &x, &y);
  const ScriptContext& ctx = context(mrb);
  const ObjectId id = scene(mrb).main_camera();
  if (id == kNoObject || ctx.screen_width <= 0 || ctx.screen_height <= 0) return mrb_nil_value();
  const Transform world = scene(mrb).world_transform(id);
  const float fov = scene(mrb).at(id).camera->fov_y_radians;
  const float tan_half = std::tan((fov > 0.0f ? fov : degrees_to_radians(config::kDefaultFovYDegrees)) * kHalf);
  const float aspect = static_cast<float>(ctx.screen_width) / static_cast<float>(ctx.screen_height);
  // From pixels to -1..1 across the screen, up positive.
  const float across = (static_cast<float>(x) / static_cast<float>(ctx.screen_width)) * kFullSpan - kHalfSpan;
  const float upward = kHalfSpan - (static_cast<float>(y) / static_cast<float>(ctx.screen_height)) * kFullSpan;
  const Vec3 direction =
      normalize(world.forward() + world.right() * (across * tan_half * aspect) + world.up() * (upward * tan_half));
  mrb_value ray = mrb_ary_new_capa(mrb, 2);
  mrb_ary_push(mrb, ray, vec3_value(mrb, world.position));
  mrb_ary_push(mrb, ray, vec3_value(mrb, direction));
  return ray;
}

// The main camera's field of view, top to bottom, in degrees.
mrb_value camera_fov(mrb_state* mrb, mrb_value) {
  const ObjectId id = scene(mrb).main_camera();
  if (id == kNoObject) return mrb_nil_value();
  const float fov = scene(mrb).at(id).camera->fov_y_radians;
  return mrb_float_value(mrb, fov > 0.0f ? radians_to_degrees(fov) : config::kDefaultFovYDegrees);
}

mrb_value camera_set_fov(mrb_state* mrb, mrb_value) {
  mrb_float degrees = 0.0;
  mrb_get_args(mrb, "f", &degrees);
  if (degrees < kMinFovDegrees || degrees > kMaxFovDegrees) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "Camera.fov must be between 10 and 150 degrees");
  }
  const ObjectId id = scene(mrb).main_camera();
  if (id != kNoObject) scene(mrb).at(id).camera->fov_y_radians = degrees_to_radians(static_cast<float>(degrees));
  return mrb_float_value(mrb, degrees);
}

mrb_value camera_motion_blur(mrb_state* mrb, mrb_value) { return mrb_float_value(mrb, context(mrb).motion_blur); }

mrb_value camera_set_motion_blur(mrb_state* mrb, mrb_value) {
  mrb_float amount = 0.0;
  mrb_get_args(mrb, "f", &amount);
  context(mrb).motion_blur = static_cast<float>(amount < 0.0 ? 0.0 : amount > kFullBlur ? kFullBlur : amount);
  return mrb_float_value(mrb, amount);
}

}  // namespace

void install_scene_bindings(mrb_state* mrb) {
  RClass* game_object = mrb_define_class(mrb, "GameObject", mrb->object_class);
  mrb_define_method(mrb, game_object, "name", game_object_name, MRB_ARGS_NONE());
  mrb_define_method(mrb, game_object, "tag", game_object_tag, MRB_ARGS_NONE());
  mrb_define_method(mrb, game_object, "alive?", game_object_alive, MRB_ARGS_NONE());
  mrb_define_method(mrb, game_object, "destroy", game_object_destroy, MRB_ARGS_NONE());
  mrb_define_method(mrb, game_object, "props", game_object_props, MRB_ARGS_NONE());
  mrb_define_method(mrb, game_object, "visible", game_object_visible, MRB_ARGS_NONE());
  mrb_define_method(mrb, game_object, "visible=", game_object_set_visible, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, game_object, "animation_offset", game_object_animation_offset, MRB_ARGS_NONE());
  mrb_define_method(mrb, game_object, "animation_offset=", game_object_set_animation_offset, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, game_object, "parent", game_object_parent, MRB_ARGS_NONE());
  mrb_define_method(mrb, game_object, "children", game_object_children, MRB_ARGS_NONE());
  mrb_define_method(mrb, game_object, "__add_component", game_object_add_component, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, mrb->kernel_module, "__require_class", kernel_require_class, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, game_object, "__has_character?", game_object_has_character, MRB_ARGS_NONE());
  mrb_define_method(mrb, game_object, "__has_rigidbody?", game_object_has_rigidbody, MRB_ARGS_NONE());
  mrb_define_method(mrb, game_object, "__has_vehicle?", game_object_has_vehicle, MRB_ARGS_NONE());

  RClass* transform = mrb_define_class(mrb, "Transform", mrb->object_class);
  mrb_define_method(mrb, transform, "position", transform_position, MRB_ARGS_NONE());
  mrb_define_method(mrb, transform, "position=", transform_set_position, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, transform, "local_position", transform_local_position, MRB_ARGS_NONE());
  mrb_define_method(mrb, transform, "local_position=", transform_set_local_position, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, transform, "rotation", transform_rotation, MRB_ARGS_NONE());
  mrb_define_method(mrb, transform, "rotation=", transform_set_rotation, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, transform, "rotate", transform_rotate, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, transform, "look_at", transform_look_at, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, transform, "scale", transform_scale, MRB_ARGS_NONE());
  mrb_define_method(mrb, transform, "scale=", transform_set_scale, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, transform, "forward", transform_forward, MRB_ARGS_NONE());
  mrb_define_method(mrb, transform, "right", transform_right, MRB_ARGS_NONE());
  mrb_define_method(mrb, transform, "up", transform_up, MRB_ARGS_NONE());

  RClass* scene_module = mrb_define_module(mrb, "Scene");
  mrb_define_module_function(mrb, scene_module, "find", scene_find, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, scene_module, "find_all", scene_find_all, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, scene_module, "objects", scene_objects, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, scene_module, "__spawn", scene_spawn, MRB_ARGS_ARG(1, 2));

  RClass* camera_module = mrb_define_module(mrb, "Camera");
  mrb_define_module_function(mrb, camera_module, "main", camera_main, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, camera_module, "screen_ray", camera_screen_ray, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, camera_module, "fov", camera_fov, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, camera_module, "fov=", camera_set_fov, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, camera_module, "motion_blur", camera_motion_blur, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, camera_module, "motion_blur=", camera_set_motion_blur, MRB_ARGS_REQ(1));
}

}  // namespace redgem
