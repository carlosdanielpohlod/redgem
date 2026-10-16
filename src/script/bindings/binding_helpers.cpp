#include "script/bindings/binding_helpers.hpp"

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/hash.h>
#include <mruby/string.h>
#include <mruby/variable.h>

#include <cmath>
#include <cstdio>
#include <string>

#include "script/script_host.hpp"

namespace redgem::binding {
namespace {

constexpr mrb_int kVectorComponents = 3;
constexpr float kMaxColorChannel = 255.0f;
// Where a Vector3 keeps its components, in the order (x, y, z) they are stored in.
constexpr const char* kComponentVariables[] = {"@x", "@y", "@z"};

constexpr std::size_t kNumberTextSize = 32;

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
  float components[kVectorComponents] = {};
  for (mrb_int i = 0; i < kVectorComponents; ++i) {
    components[i] = float_from(mrb, mrb_iv_get(mrb, vector, mrb_intern_cstr(mrb, kComponentVariables[i])));
  }
  return vec3_from(components);
}

// An Array of three: [x, y, z].
bool is_triple(mrb_value value) { return mrb_array_p(value) && RARRAY_LEN(value) == kVectorComponents; }

}  // namespace

ScriptContext& context(mrb_state* mrb) { return script_host(mrb).context(); }
Scene& scene(mrb_state* mrb) { return *context(mrb).scene; }

GameObject* bound_object(mrb_state* mrb, mrb_value self) {
  const mrb_value id = mrb_iv_get(mrb, self, mrb_intern_cstr(mrb, kIdVariable));
  const mrb_value serial = mrb_iv_get(mrb, self, mrb_intern_cstr(mrb, kSerialVariable));
  const bool bound = mrb_integer_p(id) && mrb_integer_p(serial);
  if (!bound) mrb_raise(mrb, E_TYPE_ERROR, "object is not bound to the scene");
  Scene& objects = scene(mrb);
  if (!is_index_of(mrb_integer(id), objects.size())) return nullptr;
  GameObject& object = objects.at(static_cast<ObjectId>(mrb_integer(id)));
  return object.serial == static_cast<ObjectSerial>(mrb_integer(serial)) ? &object : nullptr;
}

GameObject& alive_object(mrb_state* mrb, mrb_value self) {
  GameObject* object = bound_object(mrb, self);
  if (object == nullptr || object->destroyed) {
    const mrb_value name = mrb_iv_get(mrb, self, mrb_intern_cstr(mrb, kNameVariable));
    const mrb_value id = mrb_iv_get(mrb, self, mrb_intern_cstr(mrb, kIdVariable));
    mrb_raisef(mrb, E_RUNTIME_ERROR, "game object %v (id %v) was destroyed", name, id);
  }
  return *object;
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

mrb_value vec3_value(mrb_state* mrb, Vec3 v) {
  RClass* vector_class = mrb_class_get(mrb, kVector3Class);
  mrb_value out = mrb_obj_value(mrb_obj_alloc(mrb, MRB_TT_OBJECT, vector_class));
  const float components[kVectorComponents] = {v.x, v.y, v.z};
  for (mrb_int i = 0; i < kVectorComponents; ++i) {
    mrb_iv_set(mrb, out, mrb_intern_cstr(mrb, kComponentVariables[i]), mrb_float_value(mrb, components[i]));
  }
  return out;
}

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

mrb_float finite(mrb_state* mrb, mrb_float value) {
  if (!std::isfinite(value)) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "%n: expected a finite number, not %v", mrb_get_mid(mrb),
               mrb_float_value(mrb, value));
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
  return {static_cast<float>(r) / kMaxColorChannel, static_cast<float>(g) / kMaxColorChannel,
          static_cast<float>(b) / kMaxColorChannel, static_cast<float>(a) / kMaxColorChannel};
}

}  // namespace redgem::binding
