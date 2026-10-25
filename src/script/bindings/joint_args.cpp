#include "script/bindings/joint_args.hpp"

#include <mruby/array.h>
#include <mruby/hash.h>

#include <optional>
#include <string>
#include <utility>

#include "script/bindings/binding_helpers.hpp"

namespace redgem::binding {
namespace {

// What the kinds of joint are called in Ruby.
struct JointName {
  JointKind kind;
  const char* name;
};
constexpr JointName kJointNames[] = {
    {JointKind::Fixed, "fixed"}, {JointKind::Hinge, "hinge"},       {JointKind::Slider, "slider"},
    {JointKind::Point, "point"}, {JointKind::Distance, "distance"},
};

constexpr mrb_int kLimitEnds = 2;             // [low, high]
constexpr mrb_float kLongestSlide = 10000.0;  // meters a slider's limits may reach either way

std::optional<JointKind> joint_named(const std::string& name) {
  for (const JointName& known : kJointNames) {
    if (name == known.name) return known.kind;
  }
  return std::nullopt;
}

mrb_value option(mrb_state* mrb, mrb_value options, const char* key) {
  return mrb_hash_get(mrb, options, mrb_symbol_value(mrb_intern_cstr(mrb, key)));
}

float number_or(mrb_state* mrb, mrb_value options, const char* key, float fallback) {
  const mrb_value value = option(mrb, options, key);
  return mrb_nil_p(value) ? fallback : static_cast<float>(finite(mrb, mrb_as_float(mrb, value)));
}

Vec3 vector_or(mrb_state* mrb, mrb_value options, const char* key, Vec3 fallback) {
  const mrb_value value = option(mrb, options, key);
  return mrb_nil_p(value) ? fallback : to_vec3(mrb, value);
}

// A number of a joint's that is never below zero.
float not_negative(mrb_state* mrb, mrb_value options, const char* key, const char* what) {
  return static_cast<float>(at_least(mrb, number_or(mrb, options, key, 0.0f), 0.0, what));
}

Vec3 position_of(mrb_state* mrb, mrb_value object) {
  return scene(mrb).world_transform(alive_id(mrb, object)).position;
}

bool is_pair(mrb_value value) { return mrb_array_p(value) && RARRAY_LEN(value) == kLimitEnds; }

// [low, high], low from `lowest` to 0, high from 0 to `highest`, low below high.
std::pair<float, float> limits_of(mrb_state* mrb, mrb_value value, mrb_float lowest, mrb_float highest) {
  if (!is_pair(value)) mrb_raise(mrb, E_ARGUMENT_ERROR, "a joint's limits are [low, high]");
  const mrb_float low = from_to(mrb, mrb_as_float(mrb, mrb_ary_entry(value, 0)), lowest, 0.0, "a joint's low limit");
  const mrb_float high = from_to(mrb, mrb_as_float(mrb, mrb_ary_entry(value, 1)), 0.0, highest, "its high limit");
  if (low >= high) mrb_raise(mrb, E_ARGUMENT_ERROR, "a joint's low limit must be below its high one");
  return {static_cast<float>(low), static_cast<float>(high)};
}

bool takes_limits(JointKind kind) { return kind == JointKind::Hinge || kind == JointKind::Slider; }

// Hinges turn between limits in degrees (half a turn each way at most), sliders slide in meters.
void read_limits(mrb_state* mrb, mrb_value options, JointDesc& desc) {
  if (!takes_limits(desc.kind)) return;
  const mrb_value limits = option(mrb, options, "limits");
  if (mrb_nil_p(limits)) return;
  if (desc.kind == JointKind::Hinge) {
    const auto [low, high] = limits_of(mrb, limits, -kDegreesPerHalfTurn, kDegreesPerHalfTurn);
    desc.limits = std::make_pair(degrees_to_radians(low), degrees_to_radians(high));
  } else {
    desc.limits = limits_of(mrb, limits, -kLongestSlide, kLongestSlide);
  }
}

void read_axis(mrb_state* mrb, mrb_value options, JointDesc& desc) {
  if (!takes_limits(desc.kind)) return;  // (the kinds with limits are the kinds with an axis)
  desc.axis = vector_or(mrb, options, "axis", Vec3::up());
  if (length(desc.axis) <= kEpsilon) mrb_raise(mrb, E_ARGUMENT_ERROR, "a joint's axis must not be zero");
}

bool gives_speed_alone(mrb_state* mrb, mrb_value options) {
  return !mrb_nil_p(option(mrb, options, "motor_speed")) && mrb_nil_p(option(mrb, options, "motor_torque"));
}

// A motor turns with the torque it is given: none given, it would not turn at all.
void read_motor(mrb_state* mrb, mrb_value options, JointDesc& desc) {
  if (gives_speed_alone(mrb, options)) mrb_raise(mrb, E_ARGUMENT_ERROR, "a hinge's motor_speed needs a motor_torque");
  desc.motor_speed = degrees_to_radians(number_or(mrb, options, "motor_speed", 0.0f));
  desc.motor_torque = not_negative(mrb, options, "motor_torque", "a hinge's motor torque");
}

// A rope by default: as long as the ends are apart now, and free to come closer.
void read_distance(mrb_state* mrb, mrb_value options, JointDesc& desc) {
  const float apart = length(desc.anchor_b - desc.anchor);
  desc.min_distance = not_negative(mrb, options, "min", "a distance joint's min");
  desc.max_distance = static_cast<float>(
      at_least(mrb, number_or(mrb, options, "max", apart), desc.min_distance, "a distance joint's max"));
  desc.spring_frequency = not_negative(mrb, options, "spring", "a distance joint's spring");
  desc.spring_damping = not_negative(mrb, options, "damping", "a distance joint's damping");
}

void read_ends(mrb_state* mrb, mrb_value object, mrb_value other, mrb_value options, JointDesc& desc) {
  desc.a = alive_id(mrb, object);
  desc.b = mrb_nil_p(other) ? kNoObject : alive_id(mrb, other);
  desc.anchor = vector_or(mrb, options, "anchor", position_of(mrb, object));
  const Vec3 other_end = mrb_nil_p(other) ? desc.anchor : position_of(mrb, other);
  desc.anchor_b = vector_or(mrb, options, "other_anchor", other_end);
}

}  // namespace

JointDesc joint_desc_arg(mrb_state* mrb, mrb_value kind, mrb_value object, mrb_value other, mrb_value options) {
  const std::string name = to_name(mrb, kind);
  JointDesc desc;
  desc.kind = known(mrb, joint_named(name), "unknown joint '%s' (fixed, hinge, slider, point, distance)", name);
  read_ends(mrb, object, other, options, desc);
  read_axis(mrb, options, desc);
  read_limits(mrb, options, desc);
  read_motor(mrb, options, desc);
  if (desc.kind == JointKind::Distance) read_distance(mrb, options, desc);
  desc.break_force = break_force_arg(mrb, number_or(mrb, options, "break_force", 0.0f));
  return desc;
}

float break_force_arg(mrb_state* mrb, mrb_float force) {
  return static_cast<float>(at_least(mrb, force, 0.0, "a joint's break force"));
}

}  // namespace redgem::binding
