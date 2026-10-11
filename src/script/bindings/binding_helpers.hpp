#pragma once

// Helpers for native methods. Errors are raised as Ruby exceptions (never C++ exceptions),
// so they can only be used inside functions called from Ruby.

#include <mruby.h>

#include <string>

#include "core/math.hpp"
#include "scene/scene.hpp"
#include "scene/scene_desc.hpp"
#include "script/script_context.hpp"

namespace redgem::binding {

ScriptContext& context(mrb_state* mrb);
Scene& scene(mrb_state* mrb);

// Object id stored in @id of a wrapper (GameObject, Transform, Rigidbody...).
ObjectId self_id(mrb_state* mrb, mrb_value self);
// Any object that ever existed, destroyed or not. Raises for ids that never existed.
GameObject& object_at(mrb_state* mrb, mrb_value self);
// Raises if the object behind `self` was destroyed.
GameObject& alive_object(mrb_state* mrb, mrb_value self);

// Accepts a Vector3 or a 3-element Array.
Vec3 to_vec3(mrb_state* mrb, mrb_value value);
mrb_value vec3_value(mrb_state* mrb, Vec3 v);

// Accepts a Symbol or a String.
std::string to_name(mrb_state* mrb, mrb_value value);
// The single argument of a native method, as a name (a Symbol or a String).
std::string name_arg(mrb_state* mrb);

// A color from its channels, each 0 to 255 as Ruby passes them.
Color color_from(mrb_int r, mrb_int g, mrb_int b, mrb_int a);

mrb_value object_or_nil(mrb_state* mrb, ObjectId object);

}  // namespace redgem::binding
