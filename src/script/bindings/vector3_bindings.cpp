// Vector3, made in C++: its three numbers live inside the Ruby object itself (an mruby inline
// struct), so making one costs a single allocation and no instance variables, and its arithmetic
// runs without calling back into Ruby. The conveniences built on these are in ruby/engine/vector3.rb.
//
// Every operator makes a new Vector3, as Ruby code expects of numbers. The `!` methods change the
// vector they are called on instead, for loops that would otherwise make thousands a frame.

#include <mruby.h>
#include <mruby/class.h>
#include <mruby/istruct.h>

#include <cmath>

#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using binding::Vector3Value;

static_assert(sizeof(Vector3Value) <= ISTRUCT_DATA_SIZE, "a Vector3's three numbers fit inside its Ruby object");

constexpr mrb_float kShortest = 1e-6;  // a vector shorter than this has no direction to normalize to

Vector3Value& self_vector(mrb_value self) { return binding::vector3_of(self); }

// The Vector3 a method is given: a TypeError for anything else.
const Vector3Value& vector_arg(mrb_state* mrb) {
  const mrb_value other = binding::single_arg(mrb);
  if (!binding::is_vector3(mrb, other)) mrb_raise(mrb, E_TYPE_ERROR, "expected a Vector3");
  return binding::vector3_of(other);
}

mrb_value made(mrb_state* mrb, mrb_float x, mrb_float y, mrb_float z) { return binding::new_vector3(mrb, {x, y, z}); }

mrb_float dot(const Vector3Value& a, const Vector3Value& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

mrb_value vector3_initialize(mrb_state* mrb, mrb_value self) {
  Vector3Value& v = self_vector(self);
  v = {};
  mrb_get_args(mrb, "|fff", &v.x, &v.y, &v.z);
  return self;
}

mrb_value vector3_x(mrb_state* mrb, mrb_value self) { return mrb_float_value(mrb, self_vector(self).x); }
mrb_value vector3_y(mrb_state* mrb, mrb_value self) { return mrb_float_value(mrb, self_vector(self).y); }
mrb_value vector3_z(mrb_state* mrb, mrb_value self) { return mrb_float_value(mrb, self_vector(self).z); }

mrb_value vector3_plus(mrb_state* mrb, mrb_value self) {
  const Vector3Value& a = self_vector(self);
  const Vector3Value& b = vector_arg(mrb);
  return made(mrb, a.x + b.x, a.y + b.y, a.z + b.z);
}

mrb_value vector3_minus(mrb_state* mrb, mrb_value self) {
  const Vector3Value& a = self_vector(self);
  const Vector3Value& b = vector_arg(mrb);
  return made(mrb, a.x - b.x, a.y - b.y, a.z - b.z);
}

mrb_value vector3_negated(mrb_state* mrb, mrb_value self) {
  const Vector3Value& a = self_vector(self);
  return made(mrb, -a.x, -a.y, -a.z);
}

// (The other side is a number: a vector made longer or shorter.)
mrb_value vector3_times(mrb_state* mrb, mrb_value self) {
  const Vector3Value& a = self_vector(self);
  const mrb_float k = binding::float_arg(mrb);
  return made(mrb, a.x * k, a.y * k, a.z * k);
}

mrb_value vector3_divided(mrb_state* mrb, mrb_value self) {
  const Vector3Value& a = self_vector(self);
  const mrb_float k = binding::float_arg(mrb);
  return made(mrb, a.x / k, a.y / k, a.z / k);
}

mrb_value vector3_dot(mrb_state* mrb, mrb_value self) {
  return mrb_float_value(mrb, dot(self_vector(self), vector_arg(mrb)));
}

mrb_value vector3_cross(mrb_state* mrb, mrb_value self) {
  const Vector3Value& a = self_vector(self);
  const Vector3Value& b = vector_arg(mrb);
  return made(mrb, a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

mrb_value vector3_length(mrb_state* mrb, mrb_value self) {
  return mrb_float_value(mrb, std::sqrt(dot(self_vector(self), self_vector(self))));
}

mrb_value vector3_length_squared(mrb_state* mrb, mrb_value self) {
  return mrb_float_value(mrb, dot(self_vector(self), self_vector(self)));
}

// The same direction, one long; the zero vector for one too short to have a direction.
Vector3Value unit(const Vector3Value& v) {
  const mrb_float length = std::sqrt(dot(v, v));
  if (length <= kShortest) return {};
  return {v.x / length, v.y / length, v.z / length};
}

mrb_value vector3_normalized(mrb_state* mrb, mrb_value self) {
  return binding::new_vector3(mrb, unit(self_vector(self)));
}

mrb_value vector3_equal(mrb_state* mrb, mrb_value self) {
  const mrb_value other = binding::single_arg(mrb);
  if (!binding::is_vector3(mrb, other)) return mrb_false_value();
  const Vector3Value& a = self_vector(self);
  const Vector3Value& b = binding::vector3_of(other);
  return mrb_bool_value(a.x == b.x && a.y == b.y && a.z == b.z);
}

// The ones that change the vector they are called on, and hand it back.

mrb_value vector3_set(mrb_state* mrb, mrb_value self) {
  Vector3Value& v = self_vector(self);
  mrb_get_args(mrb, "fff", &v.x, &v.y, &v.z);
  return self;
}

mrb_value vector3_add_in_place(mrb_state* mrb, mrb_value self) {
  Vector3Value& a = self_vector(self);
  const Vector3Value& b = vector_arg(mrb);
  a = {a.x + b.x, a.y + b.y, a.z + b.z};
  return self;
}

mrb_value vector3_subtract_in_place(mrb_state* mrb, mrb_value self) {
  Vector3Value& a = self_vector(self);
  const Vector3Value& b = vector_arg(mrb);
  a = {a.x - b.x, a.y - b.y, a.z - b.z};
  return self;
}

mrb_value vector3_scale_in_place(mrb_state* mrb, mrb_value self) {
  Vector3Value& a = self_vector(self);
  const mrb_float k = binding::float_arg(mrb);
  a = {a.x * k, a.y * k, a.z * k};
  return self;
}

mrb_value vector3_normalize_in_place(mrb_state*, mrb_value self) {
  self_vector(self) = unit(self_vector(self));
  return self;
}

void install_vector3_reading(mrb_state* mrb, RClass* vector) {
  mrb_define_method(mrb, vector, "initialize", vector3_initialize, MRB_ARGS_OPT(3));
  mrb_define_method(mrb, vector, "x", vector3_x, MRB_ARGS_NONE());
  mrb_define_method(mrb, vector, "y", vector3_y, MRB_ARGS_NONE());
  mrb_define_method(mrb, vector, "z", vector3_z, MRB_ARGS_NONE());
  mrb_define_method(mrb, vector, "==", vector3_equal, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, vector, "dot", vector3_dot, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, vector, "length", vector3_length, MRB_ARGS_NONE());
  mrb_define_method(mrb, vector, "length_squared", vector3_length_squared, MRB_ARGS_NONE());
}

void install_vector3_making(mrb_state* mrb, RClass* vector) {
  mrb_define_method(mrb, vector, "+", vector3_plus, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, vector, "-", vector3_minus, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, vector, "-@", vector3_negated, MRB_ARGS_NONE());
  mrb_define_method(mrb, vector, "*", vector3_times, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, vector, "/", vector3_divided, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, vector, "cross", vector3_cross, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, vector, "normalized", vector3_normalized, MRB_ARGS_NONE());
}

void install_vector3_changing(mrb_state* mrb, RClass* vector) {
  mrb_define_method(mrb, vector, "set", vector3_set, MRB_ARGS_REQ(3));
  mrb_define_method(mrb, vector, "add!", vector3_add_in_place, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, vector, "sub!", vector3_subtract_in_place, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, vector, "scale!", vector3_scale_in_place, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, vector, "normalize!", vector3_normalize_in_place, MRB_ARGS_NONE());
}

}  // namespace

void install_vector3_bindings(mrb_state* mrb) {
  RClass* vector = mrb_define_class(mrb, binding::kVector3Class, mrb->object_class);
  MRB_SET_INSTANCE_TT(vector, MRB_TT_ISTRUCT);  // Vector3.new makes one with its numbers inside
  install_vector3_reading(mrb, vector);
  install_vector3_making(mrb, vector);
  install_vector3_changing(mrb, vector);
}

}  // namespace redgem
