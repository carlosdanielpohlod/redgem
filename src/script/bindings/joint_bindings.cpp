// Joint natives: Physics.__joint makes one, and a Joint (ruby/engine/joint.rb) works the one it
// stands for. A Joint keeps its id and its first object: once that object is gone (destroyed, or
// the scene switched), the joint is too, whatever the new scene's joints are numbered.

#include <mruby.h>
#include <mruby/variable.h>

#include <string>
#include <variant>

#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"
#include "script/bindings/joint_args.hpp"

namespace redgem {
namespace {

using namespace binding;

// Where a Joint keeps what it stands for.
constexpr const char* kObjectVariable = "@object";

PhysicsWorld& physics(mrb_state* mrb) { return *context(mrb).physics; }

JointId stored_id(mrb_state* mrb, mrb_value self) {
  return static_cast<JointId>(mrb_integer(mrb_iv_get(mrb, self, mrb_intern_cstr(mrb, kIdVariable))));
}

// Whether the joint's first object is gone, and the joint with it.
bool object_gone(mrb_state* mrb, mrb_value self) {
  const mrb_value object = mrb_iv_get(mrb, self, mrb_intern_cstr(mrb, kObjectVariable));
  return scene(mrb).resolve(wrapped_handle(mrb, object)) == kNoObject;
}

// The joint a Joint stands for; kNoJoint once its first object is gone.
JointId joint_of(mrb_state* mrb, mrb_value self) { return object_gone(mrb, self) ? kNoJoint : stored_id(mrb, self); }

// __joint(kind, object, other, options): the new joint's id.
mrb_value physics_joint(mrb_state* mrb, mrb_value) {
  mrb_value kind, object, other, options;
  mrb_get_args(mrb, "oooH", &kind, &object, &other, &options);
  const std::variant<JointId, std::string> made =
      physics(mrb).add_joint(joint_desc_arg(mrb, kind, object, other, options));
  if (const auto* problem = std::get_if<std::string>(&made)) mrb_raise(mrb, E_ARGUMENT_ERROR, problem->c_str());
  return mrb_int_value(mrb, static_cast<mrb_int>(std::get<JointId>(made)));
}

mrb_value joint_alive(mrb_state* mrb, mrb_value self) {
  return mrb_bool_value(physics(mrb).has_joint(joint_of(mrb, self)));
}

// (A broken joint is gone: what is asked of it is its id.)
mrb_value joint_broken(mrb_state* mrb, mrb_value self) {
  if (object_gone(mrb, self)) return mrb_false_value();
  return mrb_bool_value(physics(mrb).joint_broken(stored_id(mrb, self)));
}

mrb_value joint_remove(mrb_state* mrb, mrb_value self) {
  physics(mrb).remove_joint(joint_of(mrb, self));
  return mrb_nil_value();
}

mrb_value joint_enabled(mrb_state* mrb, mrb_value self) {
  return mrb_bool_value(physics(mrb).joint_enabled(joint_of(mrb, self)));
}

mrb_value joint_set_enabled(mrb_state* mrb, mrb_value self) {
  const bool enabled = bool_arg(mrb);
  physics(mrb).set_joint_enabled(joint_of(mrb, self), enabled);
  return mrb_bool_value(enabled);
}

mrb_value joint_break_force(mrb_state* mrb, mrb_value self) {
  return mrb_float_value(mrb, physics(mrb).joint_break_force(joint_of(mrb, self)));
}

mrb_value joint_set_break_force(mrb_state* mrb, mrb_value self) {
  const float force = break_force_arg(mrb, float_arg(mrb));
  physics(mrb).set_joint_break_force(joint_of(mrb, self), force);
  return mrb_float_value(mrb, force);
}

// motor(speed, torque): a hinge turns at `speed` degrees per second, with at most `torque`.
mrb_value joint_motor(mrb_state* mrb, mrb_value self) {
  mrb_float speed = 0.0, torque = 0.0;
  mrb_get_args(mrb, "ff", &speed, &torque);
  const auto turning = degrees_to_radians(static_cast<float>(finite(mrb, speed)));
  const auto most = static_cast<float>(at_least(mrb, torque, 0.0, "a hinge's motor torque"));
  physics(mrb).set_hinge_motor(joint_of(mrb, self), turning, most);
  return mrb_nil_value();
}

mrb_value joint_angle(mrb_state* mrb, mrb_value self) {
  return mrb_float_value(mrb, radians_to_degrees(physics(mrb).hinge_angle(joint_of(mrb, self))));
}

}  // namespace

void install_joint_bindings(mrb_state* mrb) {
  mrb_define_module_function(mrb, mrb_module_get(mrb, "Physics"), "__joint", physics_joint, MRB_ARGS_REQ(4));
  RClass* joint = mrb_define_class(mrb, "Joint", mrb->object_class);
  mrb_define_method(mrb, joint, "alive?", joint_alive, MRB_ARGS_NONE());
  mrb_define_method(mrb, joint, "broken?", joint_broken, MRB_ARGS_NONE());
  mrb_define_method(mrb, joint, "remove", joint_remove, MRB_ARGS_NONE());
  mrb_define_method(mrb, joint, "enabled?", joint_enabled, MRB_ARGS_NONE());
  mrb_define_method(mrb, joint, "enabled=", joint_set_enabled, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, joint, "break_force", joint_break_force, MRB_ARGS_NONE());
  mrb_define_method(mrb, joint, "break_force=", joint_set_break_force, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, joint, "__motor", joint_motor, MRB_ARGS_REQ(2));
  mrb_define_method(mrb, joint, "__angle", joint_angle, MRB_ARGS_NONE());
}

}  // namespace redgem
