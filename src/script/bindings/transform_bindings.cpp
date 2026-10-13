// Transform natives.

#include <mruby.h>

#include "core/log.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using namespace binding;

void notify_transform_written(mrb_state* mrb, ObjectId id, TransformWrite write) {
  ScriptContext& ctx = context(mrb);
  if (ctx.on_transform_written) ctx.on_transform_written(id, write);
}

// Turns an object in the world, and lets whoever follows transforms know.
void write_rotation(mrb_state* mrb, ObjectId id, Quat rotation) {
  scene(mrb).set_world_rotation(id, rotation);
  notify_transform_written(mrb, id, TransformWrite::Rotation);
}

Transform world_of(mrb_state* mrb, mrb_value self) { return scene(mrb).world_transform(alive_id(mrb, self)); }

mrb_value transform_position(mrb_state* mrb, mrb_value self) { return vec3_value(mrb, world_of(mrb, self).position); }

mrb_value transform_set_position(mrb_state* mrb, mrb_value self) {
  const mrb_value value = single_arg(mrb);
  const ObjectId id = alive_id(mrb, self);
  scene(mrb).set_world_position(id, to_vec3(mrb, value));
  notify_transform_written(mrb, id, TransformWrite::Position);
  return value;
}

mrb_value transform_local_position(mrb_state* mrb, mrb_value self) {
  return vec3_value(mrb, alive_object(mrb, self).local.position);
}

mrb_value transform_set_local_position(mrb_state* mrb, mrb_value self) {
  const mrb_value value = single_arg(mrb);
  GameObject& object = alive_object(mrb, self);
  object.local.position = to_vec3(mrb, value);
  notify_transform_written(mrb, object.id, TransformWrite::Position);
  return value;
}

mrb_value transform_rotation(mrb_state* mrb, mrb_value self) {
  return vec3_value(mrb, quat_to_euler_degrees(world_of(mrb, self).rotation));
}

mrb_value transform_set_rotation(mrb_state* mrb, mrb_value self) {
  const mrb_value value = single_arg(mrb);
  write_rotation(mrb, alive_id(mrb, self), quat_from_euler_degrees(to_vec3(mrb, value)));
  return value;
}

mrb_value transform_rotate(mrb_state* mrb, mrb_value self) {
  const mrb_value value = single_arg(mrb);
  const ObjectId id = alive_id(mrb, self);
  // Rotation in the object's own space.
  const Quat delta = quat_from_euler_degrees(to_vec3(mrb, value));
  write_rotation(mrb, id, normalize(scene(mrb).world_transform(id).rotation * delta));
  return self;
}

// A place in the world given as a GameObject (where it is) or as a Vector3.
Vec3 point_of(mrb_state* mrb, mrb_value target) {
  if (!is_game_object(mrb, target)) return to_vec3(mrb, target);
  return scene(mrb).world_transform(alive_id(mrb, target)).position;
}

mrb_value transform_look_at(mrb_state* mrb, mrb_value self) {
  const mrb_value target = single_arg(mrb);
  const ObjectId id = alive_id(mrb, self);
  const Vec3 direction = point_of(mrb, target) - scene(mrb).world_transform(id).position;
  write_rotation(mrb, id, quat_look_rotation(direction, Vec3::up()));
  return self;
}

mrb_value transform_scale(mrb_state* mrb, mrb_value self) {
  return vec3_value(mrb, alive_object(mrb, self).local.scale);
}

mrb_value transform_set_scale(mrb_state* mrb, mrb_value self) {
  const mrb_value value = single_arg(mrb);
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

void install_placement_bindings(RClass* transform, mrb_state* mrb) {
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
}

void install_direction_bindings(RClass* transform, mrb_state* mrb) {
  mrb_define_method(mrb, transform, "forward", transform_forward, MRB_ARGS_NONE());
  mrb_define_method(mrb, transform, "right", transform_right, MRB_ARGS_NONE());
  mrb_define_method(mrb, transform, "up", transform_up, MRB_ARGS_NONE());
}

}  // namespace

void install_transform_bindings(mrb_state* mrb) {
  RClass* transform = mrb_define_class(mrb, "Transform", mrb->object_class);
  install_placement_bindings(transform, mrb);
  install_direction_bindings(transform, mrb);
}

}  // namespace redgem
