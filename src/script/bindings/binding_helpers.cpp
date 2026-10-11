#include "script/bindings/binding_helpers.hpp"

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/string.h>
#include <mruby/variable.h>

#include "script/script_host.hpp"

namespace redgem::binding {
namespace {

constexpr mrb_int kVectorComponents = 3;
constexpr float kMaxColorChannel = 255.0f;

}  // namespace

ScriptContext& context(mrb_state* mrb) { return script_host(mrb).context(); }
Scene& scene(mrb_state* mrb) { return *context(mrb).scene; }

ObjectId self_id(mrb_state* mrb, mrb_value self) {
  const mrb_value id = mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@id"));
  if (!mrb_integer_p(id)) mrb_raise(mrb, E_TYPE_ERROR, "object is not bound to the scene");
  const mrb_int raw = mrb_integer(id);
  if (raw < 0 || raw >= static_cast<mrb_int>(scene(mrb).size())) {
    mrb_raisef(mrb, E_RANGE_ERROR, "no game object with id %v", id);
  }
  return static_cast<ObjectId>(raw);
}

GameObject& object_at(mrb_state* mrb, mrb_value self) { return scene(mrb).at(self_id(mrb, self)); }

GameObject& alive_object(mrb_state* mrb, mrb_value self) {
  GameObject& object = object_at(mrb, self);
  if (object.destroyed) {
    mrb_raisef(mrb, E_RUNTIME_ERROR, "game object %v was destroyed", mrb_int_value(mrb, object.id));
  }
  return object;
}

Vec3 to_vec3(mrb_state* mrb, mrb_value value) {
  if (mrb_array_p(value) && RARRAY_LEN(value) == kVectorComponents) {
    return {static_cast<float>(mrb_as_float(mrb, mrb_ary_entry(value, 0))),
            static_cast<float>(mrb_as_float(mrb, mrb_ary_entry(value, 1))),
            static_cast<float>(mrb_as_float(mrb, mrb_ary_entry(value, 2)))};
  }
  RClass* vector_class = mrb_class_get(mrb, "Vector3");
  if (!mrb_obj_is_kind_of(mrb, value, vector_class)) {
    mrb_raise(mrb, E_TYPE_ERROR, "expected a Vector3 or [x, y, z]");
  }
  return {static_cast<float>(mrb_as_float(mrb, mrb_iv_get(mrb, value, mrb_intern_lit(mrb, "@x")))),
          static_cast<float>(mrb_as_float(mrb, mrb_iv_get(mrb, value, mrb_intern_lit(mrb, "@y")))),
          static_cast<float>(mrb_as_float(mrb, mrb_iv_get(mrb, value, mrb_intern_lit(mrb, "@z"))))};
}

mrb_value vec3_value(mrb_state* mrb, Vec3 v) {
  RClass* vector_class = mrb_class_get(mrb, "Vector3");
  mrb_value out = mrb_obj_value(mrb_obj_alloc(mrb, MRB_TT_OBJECT, vector_class));
  mrb_iv_set(mrb, out, mrb_intern_lit(mrb, "@x"), mrb_float_value(mrb, v.x));
  mrb_iv_set(mrb, out, mrb_intern_lit(mrb, "@y"), mrb_float_value(mrb, v.y));
  mrb_iv_set(mrb, out, mrb_intern_lit(mrb, "@z"), mrb_float_value(mrb, v.z));
  return out;
}

std::string to_name(mrb_state* mrb, mrb_value value) {
  if (!mrb_symbol_p(value) && !mrb_string_p(value)) mrb_raise(mrb, E_TYPE_ERROR, "expected a Symbol or String");
  return mrb_str_to_cstr(mrb, mrb_obj_as_string(mrb, value));
}

mrb_value object_or_nil(mrb_state* mrb, ObjectId object) {
  if (!scene(mrb).is_alive(object)) return mrb_nil_value();
  return script_host(mrb).object_value(object);
}

std::string name_arg(mrb_state* mrb) {
  mrb_value name;
  mrb_get_args(mrb, "o", &name);
  return to_name(mrb, name);
}

Color color_from(mrb_int r, mrb_int g, mrb_int b, mrb_int a) {
  return {static_cast<float>(r) / kMaxColorChannel, static_cast<float>(g) / kMaxColorChannel,
          static_cast<float>(b) / kMaxColorChannel, static_cast<float>(a) / kMaxColorChannel};
}

}  // namespace redgem::binding
