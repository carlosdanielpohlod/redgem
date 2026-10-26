// What scripts do to a Rigidbody besides moving it: forces and torques, spin, mass, gravity, sleep,
// switching between dynamic and kinematic; and the collision layer of rigid bodies and characters.

#include <mruby.h>

#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"
#include "script/bindings/physics_args.hpp"

namespace redgem {
namespace {

using namespace binding;

PhysicsWorld& physics(mrb_state* mrb) { return *context(mrb).physics; }

// Scripts give spin in degrees per second, as they give rotations in degrees; Jolt in radians.
Vec3 radians_of(Vec3 degrees) { return degrees * degrees_to_radians(1.0f); }
Vec3 degrees_of(Vec3 radians) { return radians * (1.0f / degrees_to_radians(1.0f)); }

// --- Forces ------------------------------------------------------------------------------

// __add_force(force, point): at the center of mass when the point is nil.
mrb_value rigidbody_add_force(mrb_state* mrb, mrb_value self) {
  mrb_value force, point;
  mrb_get_args(mrb, "oo", &force, &point);
  const ObjectId id = alive_id(mrb, self);
  if (mrb_nil_p(point)) {
    physics(mrb).add_force(id, to_vec3(mrb, force));
  } else {
    physics(mrb).add_force_at(id, to_vec3(mrb, force), to_vec3(mrb, point));
  }
  return mrb_nil_value();
}

mrb_value rigidbody_add_torque(mrb_state* mrb, mrb_value self) {
  const mrb_value torque = single_arg(mrb);
  physics(mrb).add_torque(alive_id(mrb, self), to_vec3(mrb, torque));
  return mrb_nil_value();
}

mrb_value rigidbody_angular_velocity(mrb_state* mrb, mrb_value self) {
  return vec3_value(mrb, degrees_of(physics(mrb).angular_velocity(alive_id(mrb, self))));
}

mrb_value rigidbody_set_angular_velocity(mrb_state* mrb, mrb_value self) {
  const mrb_value velocity = single_arg(mrb);
  physics(mrb).set_angular_velocity(alive_id(mrb, self), radians_of(to_vec3(mrb, velocity)));
  return velocity;
}

// --- What it is --------------------------------------------------------------------------

mrb_value rigidbody_mass(mrb_state* mrb, mrb_value self) {
  return mrb_float_value(mrb, physics(mrb).mass(alive_id(mrb, self)));
}

mrb_value rigidbody_set_mass(mrb_state* mrb, mrb_value self) {
  const mrb_float mass = more_than(mrb, float_arg(mrb), 0.0, "a body's mass");
  physics(mrb).set_mass(alive_id(mrb, self), static_cast<float>(mass));
  return mrb_float_value(mrb, mass);
}

mrb_value rigidbody_kinematic(mrb_state* mrb, mrb_value self) {
  return mrb_bool_value(physics(mrb).body_type(alive_id(mrb, self)) == BodyType::Kinematic);
}

mrb_value rigidbody_set_kinematic(mrb_state* mrb, mrb_value self) {
  const bool kinematic = bool_arg(mrb);
  const BodyType type = kinematic ? BodyType::Kinematic : BodyType::Dynamic;
  if (!physics(mrb).set_body_type(alive_id(mrb, self), type)) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "only dynamic and kinematic bodies switch between the two (not a vehicle)");
  }
  return mrb_bool_value(kinematic);
}

mrb_value rigidbody_gravity_scale(mrb_state* mrb, mrb_value self) {
  return mrb_float_value(mrb, physics(mrb).gravity_scale(alive_id(mrb, self)));
}

mrb_value rigidbody_set_gravity_scale(mrb_state* mrb, mrb_value self) {
  const mrb_float scale = float_arg(mrb);
  physics(mrb).set_gravity_scale(alive_id(mrb, self), static_cast<float>(scale));
  return mrb_float_value(mrb, scale);
}

// --- Sleep -------------------------------------------------------------------------------

mrb_value rigidbody_sleep(mrb_state* mrb, mrb_value self) {
  physics(mrb).set_sleeping(alive_id(mrb, self), true);
  return mrb_nil_value();
}

mrb_value rigidbody_wake(mrb_state* mrb, mrb_value self) {
  physics(mrb).set_sleeping(alive_id(mrb, self), false);
  return mrb_nil_value();
}

mrb_value rigidbody_sleeping(mrb_state* mrb, mrb_value self) {
  return mrb_bool_value(physics(mrb).sleeping(alive_id(mrb, self)));
}

// --- Collision layer (rigid bodies and characters) ------------------------------------------

mrb_value body_layer(mrb_state* mrb, mrb_value self) {
  return layer_value(mrb, physics(mrb).layer(alive_id(mrb, self)));
}

mrb_value body_set_layer(mrb_state* mrb, mrb_value self) {
  const mrb_value name = single_arg(mrb);
  physics(mrb).set_layer(alive_id(mrb, self), layer_arg(mrb, name));
  return name;
}

void install_force_bindings(RClass* rigidbody, mrb_state* mrb) {
  mrb_define_method(mrb, rigidbody, "__add_force", rigidbody_add_force, MRB_ARGS_REQ(2));
  mrb_define_method(mrb, rigidbody, "add_torque", rigidbody_add_torque, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, rigidbody, "angular_velocity", rigidbody_angular_velocity, MRB_ARGS_NONE());
  mrb_define_method(mrb, rigidbody, "angular_velocity=", rigidbody_set_angular_velocity, MRB_ARGS_REQ(1));
}

void install_state_bindings(RClass* rigidbody, mrb_state* mrb) {
  mrb_define_method(mrb, rigidbody, "mass", rigidbody_mass, MRB_ARGS_NONE());
  mrb_define_method(mrb, rigidbody, "mass=", rigidbody_set_mass, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, rigidbody, "kinematic?", rigidbody_kinematic, MRB_ARGS_NONE());
  mrb_define_method(mrb, rigidbody, "kinematic=", rigidbody_set_kinematic, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, rigidbody, "gravity_scale", rigidbody_gravity_scale, MRB_ARGS_NONE());
  mrb_define_method(mrb, rigidbody, "gravity_scale=", rigidbody_set_gravity_scale, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, rigidbody, "sleep", rigidbody_sleep, MRB_ARGS_NONE());
  mrb_define_method(mrb, rigidbody, "wake", rigidbody_wake, MRB_ARGS_NONE());
  mrb_define_method(mrb, rigidbody, "sleeping?", rigidbody_sleeping, MRB_ARGS_NONE());
}

void install_layer_bindings(RClass* body, mrb_state* mrb) {
  mrb_define_method(mrb, body, "layer", body_layer, MRB_ARGS_NONE());
  mrb_define_method(mrb, body, "layer=", body_set_layer, MRB_ARGS_REQ(1));
}

}  // namespace

void install_rigidbody_bindings(mrb_state* mrb) {
  RClass* rigidbody = mrb_class_get(mrb, "Rigidbody");
  install_force_bindings(rigidbody, mrb);
  install_state_bindings(rigidbody, mrb);
  install_layer_bindings(rigidbody, mrb);
  install_layer_bindings(mrb_class_get(mrb, "CharacterController"), mrb);
}

}  // namespace redgem
