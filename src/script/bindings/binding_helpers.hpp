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

// Ruby classes of the prelude that natives look up by name.
inline constexpr const char* kGameObjectClass = "GameObject";
inline constexpr const char* kVector3Class = "Vector3";
// Where a wrapper (GameObject, Transform, Rigidbody...) keeps the id of its object.
inline constexpr const char* kIdVariable = "@id";

ScriptContext& context(mrb_state* mrb);
Scene& scene(mrb_state* mrb);

// A part of the engine a native cannot do without. A context that has none (a script run outside
// a game) gets a RuntimeError saying `unavailable`.
template <typename Part>
Part& required(mrb_state* mrb, Part* part, const char* unavailable) {
  if (part == nullptr) mrb_raise(mrb, E_RUNTIME_ERROR, unavailable);
  return *part;
}

// Object id stored in @id of a wrapper (GameObject, Transform, Rigidbody...).
ObjectId self_id(mrb_state* mrb, mrb_value self);
// Any object that ever existed, destroyed or not. Raises for ids that never existed.
GameObject& object_at(mrb_state* mrb, mrb_value self);
// Raises if the object behind `self` was destroyed.
GameObject& alive_object(mrb_state* mrb, mrb_value self);
// The id of the object behind `self`, which must not have been destroyed.
ObjectId alive_id(mrb_state* mrb, mrb_value self);

// Accepts a Vector3 or a 3-element Array.
Vec3 to_vec3(mrb_state* mrb, mrb_value value);
mrb_value vec3_value(mrb_state* mrb, Vec3 v);
bool is_vector3(mrb_state* mrb, mrb_value value);
bool is_game_object(mrb_state* mrb, mrb_value value);

// A Ruby String holding `text` (which may contain any byte).
mrb_value text_value(mrb_state* mrb, const std::string& text);
// hash[:key] = value.
void set_entry(mrb_state* mrb, mrb_value hash, const char* key, mrb_value value);

// A Symbol or a String.
bool is_name(mrb_value value);
// Accepts a Symbol or a String.
std::string to_name(mrb_state* mrb, mrb_value value);
// The single argument of a native method: any value, a number, true or false, or a name (a
// Symbol or a String).
mrb_value single_arg(mrb_state* mrb);
mrb_float float_arg(mrb_state* mrb);
bool bool_arg(mrb_state* mrb);
std::string name_arg(mrb_state* mrb);

// A Float as a number that can be kept or sent; an ArgumentError saying `refusal` for infinity
// and NaN.
double finite_number(mrb_state* mrb, mrb_value value, const char* refusal);

// A color from its channels, each 0 to 255 as Ruby passes them.
Color color_from(mrb_int r, mrb_int g, mrb_int b, mrb_int a);

mrb_value object_or_nil(mrb_state* mrb, ObjectId object);

}  // namespace redgem::binding
