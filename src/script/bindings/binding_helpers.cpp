#include "script/bindings/binding_helpers.hpp"

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/hash.h>
#include <mruby/istruct.h>
#include <mruby/string.h>
#include <mruby/variable.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>

#include "scene/color_space.hpp"
#include "script/script_host.hpp"

namespace redgem::binding {
namespace {

constexpr mrb_int kVectorComponents = 3;
// The engine keeps its numbers as floats: a number from a script must fit one.
constexpr mrb_float kLargestNumber = std::numeric_limits<float>::max();

constexpr std::size_t kNumberTextSize = 32;

// A channel from 0 to kMaxColorChannel as a share from 0 to 1.
float share_of_channel(mrb_float channel) { return static_cast<float>(channel / kMaxColorChannel); }

float float_from(mrb_state* mrb, mrb_value value) { return static_cast<float>(finite(mrb, mrb_as_float(mrb, value))); }

// A number as a script would write it: 5, -0.5, 150.
std::string number_text(mrb_float value) {
  char text[kNumberTextSize];
  std::snprintf(text, sizeof text, "%g", value);
  return text;
}

[[noreturn]] void refuse(mrb_state* mrb, const char* what, const std::string& range, mrb_float value) {
  const std::string message = std::string(what) + " must be " + range + ", not " + number_text(value);
  mrb_raise(mrb, E_ARGUMENT_ERROR, message.c_str());
}

Vec3 vec3_from_array(mrb_state* mrb, mrb_value array) {
  float components[kVectorComponents] = {};
  for (mrb_int i = 0; i < kVectorComponents; ++i) components[i] = float_from(mrb, mrb_ary_entry(array, i));
  return vec3_from(components);
}

Vec3 vec3_from_vector(mrb_state* mrb, mrb_value vector) {
  const Vector3Value& v = vector3_of(vector);
  return {static_cast<float>(finite(mrb, v.x)), static_cast<float>(finite(mrb, v.y)),
          static_cast<float>(finite(mrb, v.z))};
}

// An Array of three: [x, y, z].
bool is_triple(mrb_value value) { return mrb_array_p(value) && RARRAY_LEN(value) == kVectorComponents; }

}  // namespace

ScriptContext& context(mrb_state* mrb) { return script_host(mrb).context(); }
Scene& scene(mrb_state* mrb) { return *context(mrb).scene; }

ObjectHandle wrapped_handle(mrb_state* mrb, mrb_value self) {
  const mrb_value id = mrb_iv_get(mrb, self, mrb_intern_cstr(mrb, kIdVariable));
  const mrb_value serial = mrb_iv_get(mrb, self, mrb_intern_cstr(mrb, kSerialVariable));
  const bool bound = mrb_integer_p(id) && mrb_integer_p(serial);
  if (!bound) mrb_raise(mrb, E_TYPE_ERROR, "object is not bound to the scene");
  return {static_cast<ObjectId>(mrb_integer(id)), static_cast<ObjectSerial>(mrb_integer(serial))};
}

GameObject& alive_object(mrb_state* mrb, mrb_value self) {
  const ObjectId found = scene(mrb).resolve(wrapped_handle(mrb, self));
  if (found == kNoObject) {
    const mrb_value name = mrb_iv_get(mrb, self, mrb_intern_cstr(mrb, kNameVariable));
    const mrb_value id = mrb_iv_get(mrb, self, mrb_intern_cstr(mrb, kIdVariable));
    mrb_raisef(mrb, E_RUNTIME_ERROR, "game object %v (id %v) was destroyed", name, id);
  }
  return scene(mrb).at(found);
}

ObjectId alive_id(mrb_state* mrb, mrb_value self) { return alive_object(mrb, self).id; }

bool is_vector3(mrb_state* mrb, mrb_value value) {
  return mrb_obj_is_kind_of(mrb, value, mrb_class_get(mrb, kVector3Class));
}

bool is_game_object(mrb_state* mrb, mrb_value value) {
  return mrb_obj_is_kind_of(mrb, value, mrb_class_get(mrb, kGameObjectClass));
}

Vec3 to_vec3(mrb_state* mrb, mrb_value value) {
  if (is_triple(value)) return vec3_from_array(mrb, value);
  if (!is_vector3(mrb, value)) mrb_raise(mrb, E_TYPE_ERROR, "expected a Vector3 or [x, y, z]");
  return vec3_from_vector(mrb, value);
}

Vector3Value& vector3_of(mrb_value vector) { return *static_cast<Vector3Value*>(mrb_istruct_ptr(vector)); }

mrb_value new_vector3(mrb_state* mrb, const Vector3Value& v) {
  mrb_value out = mrb_obj_value(mrb_obj_alloc(mrb, MRB_TT_ISTRUCT, mrb_class_get(mrb, kVector3Class)));
  vector3_of(out) = v;
  return out;
}

mrb_value vec3_value(mrb_state* mrb, Vec3 v) { return new_vector3(mrb, {v.x, v.y, v.z}); }

mrb_value text_value(mrb_state* mrb, const std::string& text) {
  return mrb_str_new(mrb, text.data(), static_cast<mrb_int>(text.size()));
}

void set_entry(mrb_state* mrb, mrb_value hash, const char* key, mrb_value value) {
  mrb_hash_set(mrb, hash, mrb_symbol_value(mrb_intern_cstr(mrb, key)), value);
}

bool is_name(mrb_value value) { return mrb_symbol_p(value) || mrb_string_p(value); }

std::string to_name(mrb_state* mrb, mrb_value value) {
  if (!is_name(value)) mrb_raise(mrb, E_TYPE_ERROR, "expected a Symbol or String");
  return mrb_str_to_cstr(mrb, mrb_obj_as_string(mrb, value));
}

mrb_value object_or_nil(mrb_state* mrb, ObjectId object) {
  if (!scene(mrb).is_alive(object)) return mrb_nil_value();
  return script_host(mrb).object_value(object);
}

mrb_value single_arg(mrb_state* mrb) {
  mrb_value value;
  mrb_get_args(mrb, "o", &value);
  return value;
}

bool fits_a_float(mrb_float value) { return std::isfinite(value) && std::fabs(value) <= kLargestNumber; }

mrb_float finite(mrb_state* mrb, mrb_float value) {
  if (!fits_a_float(value)) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "%n: expected a finite number no larger than %v, not %v", mrb_get_mid(mrb),
               mrb_float_value(mrb, kLargestNumber), mrb_float_value(mrb, value));
  }
  return value;
}

mrb_float at_least(mrb_state* mrb, mrb_float value, mrb_float low, const char* what) {
  if (finite(mrb, value) < low) refuse(mrb, what, "at least " + number_text(low), value);
  return value;
}

mrb_float more_than(mrb_state* mrb, mrb_float value, mrb_float low, const char* what) {
  if (finite(mrb, value) <= low) refuse(mrb, what, "more than " + number_text(low), value);
  return value;
}

mrb_float from_to(mrb_state* mrb, mrb_float value, mrb_float low, mrb_float high, const char* what) {
  const bool inside = finite(mrb, value) >= low && value <= high;
  if (!inside) refuse(mrb, what, "from " + number_text(low) + " to " + number_text(high), value);
  return value;
}

mrb_float float_arg(mrb_state* mrb) {
  mrb_float number = 0.0;
  mrb_get_args(mrb, "f", &number);
  return finite(mrb, number);
}

bool bool_arg(mrb_state* mrb) {
  mrb_bool flag = false;
  mrb_get_args(mrb, "b", &flag);
  return flag;
}

std::string name_arg(mrb_state* mrb) { return to_name(mrb, single_arg(mrb)); }

double finite_number(mrb_state* mrb, mrb_value value, const char* refusal) {
  if (!std::isfinite(mrb_float(value))) mrb_raise(mrb, E_ARGUMENT_ERROR, refusal);
  return static_cast<double>(mrb_float(value));
}

Color color_from(mrb_int r, mrb_int g, mrb_int b, mrb_int a) {
  return {share_of_channel(r), share_of_channel(g), share_of_channel(b), share_of_channel(a)};
}

float channel_from(mrb_state* mrb, mrb_float channel, const char* what) {
  return share_of_channel(from_to(mrb, channel, 0.0, kMaxColorChannel, what));
}

mrb_value channel_shown(mrb_state* mrb, float share) {
  return mrb_int_value(mrb, std::lround(std::clamp(share, 0.0f, 1.0f) * kMaxColorChannel));
}

}  // namespace redgem::binding
