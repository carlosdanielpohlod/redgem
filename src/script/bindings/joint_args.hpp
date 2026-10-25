#pragma once

// What Physics.joint says a joint is, read from what Ruby hands over and checked as every native's
// arguments are (see binding_helpers.hpp). Angles come in degrees, as rotations do everywhere in
// the scripting API.

#include <mruby.h>

#include "physics/physics_world.hpp"

namespace redgem::binding {

// `kind` is :fixed, :hinge, :slider, :point or :distance; `object` a GameObject; `other` one or
// nil (the world); `options` a Hash of anchor:, other_anchor:, axis:, limits:, motor_speed:,
// motor_torque:, min:, max:, spring:, damping:, break_force: (each may be left out).
JointDesc joint_desc_arg(mrb_state* mrb, mrb_value kind, mrb_value object, mrb_value other, mrb_value options);

// A force a joint can take before it breaks: 0 (never breaks) or more, in newtons.
float break_force_arg(mrb_state* mrb, mrb_float force);

}  // namespace redgem::binding
