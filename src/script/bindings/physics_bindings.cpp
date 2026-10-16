// CharacterController, Rigidbody and Physics natives.

#include <mruby.h>
#include <mruby/hash.h>

#include <algorithm>

#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using namespace binding;

constexpr mrb_float kDefaultRaycastDistance = 1000.0;

PhysicsWorld& physics(mrb_state* mrb) { return *context(mrb).physics; }

// --- CharacterController ---------------------------------------------------------------

mrb_value character_move(mrb_state* mrb, mrb_value self) {
  const mrb_value velocity = single_arg(mrb);
  physics(mrb).set_character_move(alive_id(mrb, self), to_vec3(mrb, velocity));
  return mrb_nil_value();
}

mrb_value character_jump(mrb_state* mrb, mrb_value self) {
  const mrb_float speed = float_arg(mrb);
  physics(mrb).character_jump(alive_id(mrb, self), static_cast<float>(speed));
  return mrb_nil_value();
}

mrb_value character_grounded(mrb_state* mrb, mrb_value self) {
  return mrb_bool_value(physics(mrb).character_grounded(alive_id(mrb, self)));
}

mrb_value body_velocity(mrb_state* mrb, mrb_value self) {
  return vec3_value(mrb, physics(mrb).velocity(alive_id(mrb, self)));
}

// --- Rigidbody ---------------------------------------------------------------------------

mrb_value rigidbody_set_velocity(mrb_state* mrb, mrb_value self) {
  const mrb_value velocity = single_arg(mrb);
  physics(mrb).set_velocity(alive_id(mrb, self), to_vec3(mrb, velocity));
  return velocity;
}

mrb_value rigidbody_add_impulse(mrb_state* mrb, mrb_value self) {
  const mrb_value impulse = single_arg(mrb);
  physics(mrb).add_impulse(alive_id(mrb, self), to_vec3(mrb, impulse));
  return mrb_nil_value();
}

// --- Vehicle -------------------------------------------------------------------------------

constexpr float kMinAxisInput = -1.0f;
constexpr float kMaxInput = 1.0f;
constexpr float kMinPedalInput = 0.0f;

float clamped(mrb_float value, float low, float high) { return std::clamp(static_cast<float>(value), low, high); }

mrb_value vehicle_drive(mrb_state* mrb, mrb_value self) {
  mrb_float throttle = 0.0, steering = 0.0, brake = 0.0, handbrake = 0.0;
  mrb_get_args(mrb, "ffff", &throttle, &steering, &brake, &handbrake);
  VehicleInput input;
  input.throttle = clamped(throttle, kMinAxisInput, kMaxInput);
  input.steering = clamped(steering, kMinAxisInput, kMaxInput);
  input.brake = clamped(brake, kMinPedalInput, kMaxInput);
  input.handbrake = clamped(handbrake, kMinPedalInput, kMaxInput);
  physics(mrb).set_vehicle_input(alive_id(mrb, self), input);
  return mrb_nil_value();
}

mrb_value vehicle_speed(mrb_state* mrb, mrb_value self) {
  return mrb_float_value(mrb, physics(mrb).vehicle_forward_speed(alive_id(mrb, self)));
}

// --- Physics -------------------------------------------------------------------------------

// What Physics.raycast answers: { object:, point:, normal:, distance: }.
mrb_value hit_value(mrb_state* mrb, const RaycastHit& hit) {
  mrb_value result = mrb_hash_new(mrb);
  set_entry(mrb, result, "object", object_or_nil(mrb, hit.object));
  set_entry(mrb, result, "point", vec3_value(mrb, hit.point));
  set_entry(mrb, result, "normal", vec3_value(mrb, hit.normal));
  set_entry(mrb, result, "distance", mrb_float_value(mrb, hit.distance));
  return result;
}

mrb_value physics_raycast(mrb_state* mrb, mrb_value) {
  mrb_value origin;
  mrb_value direction;
  mrb_float max_distance = kDefaultRaycastDistance;
  mrb_value ignore = mrb_nil_value();
  mrb_get_args(mrb, "oo|fo", &origin, &direction, &max_distance, &ignore);
  const ObjectId ignored = mrb_nil_p(ignore) ? kNoObject : alive_id(mrb, ignore);
  const auto hit =
      physics(mrb).raycast(to_vec3(mrb, origin), to_vec3(mrb, direction), static_cast<float>(max_distance), ignored);
  return hit ? hit_value(mrb, *hit) : mrb_nil_value();
}

void install_body_bindings(mrb_state* mrb) {
  RClass* character = mrb_define_class(mrb, "CharacterController", mrb->object_class);
  mrb_define_method(mrb, character, "move", character_move, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, character, "jump", character_jump, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, character, "grounded?", character_grounded, MRB_ARGS_NONE());
  mrb_define_method(mrb, character, "velocity", body_velocity, MRB_ARGS_NONE());

  RClass* rigidbody = mrb_define_class(mrb, "Rigidbody", mrb->object_class);
  mrb_define_method(mrb, rigidbody, "velocity", body_velocity, MRB_ARGS_NONE());
  mrb_define_method(mrb, rigidbody, "velocity=", rigidbody_set_velocity, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, rigidbody, "add_impulse", rigidbody_add_impulse, MRB_ARGS_REQ(1));
}

void install_vehicle_bindings(mrb_state* mrb) {
  RClass* vehicle = mrb_define_class(mrb, "Vehicle", mrb->object_class);
  mrb_define_method(mrb, vehicle, "__drive", vehicle_drive, MRB_ARGS_REQ(4));
  mrb_define_method(mrb, vehicle, "speed", vehicle_speed, MRB_ARGS_NONE());
  mrb_define_method(mrb, vehicle, "velocity", body_velocity, MRB_ARGS_NONE());
}

}  // namespace

void install_physics_bindings(mrb_state* mrb) {
  install_body_bindings(mrb);
  install_vehicle_bindings(mrb);
  RClass* physics_module = mrb_define_module(mrb, "Physics");
  mrb_define_module_function(mrb, physics_module, "raycast", physics_raycast, MRB_ARGS_ARG(2, 2));
}

}  // namespace redgem
