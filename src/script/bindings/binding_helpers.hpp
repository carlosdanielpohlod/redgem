#pragma once

// Helpers for native methods. Errors are raised as Ruby exceptions (never C++ exceptions),
// so they can only be used inside functions called from Ruby.
//
// What every native holds the scripts' arguments to:
//   - a number is finite: infinity and NaN are refused wherever they would go (float_arg,
//     to_vec3, finite for the rest), so they never reach the scene, physics or the renderer;
//   - a number with a range of its own is refused outside it, saying the range and what was given
//     (at_least, more_than, from_to), never quietly clamped (a steering input is the exception:
//     an analog stick may go a little past full, see Vehicle#drive);
//   - an object is one that is still there (alive_object, alive_id).

#include <mruby.h>

#include <optional>
#include <string>

#include "core/math.hpp"
#include "scene/scene.hpp"
#include "scene/scene_desc.hpp"
#include "script/script_context.hpp"

namespace redgem::binding {

// Ruby classes of the prelude that natives look up by name.
inline constexpr const char* kGameObjectClass = "GameObject";
inline constexpr const char* kVector3Class = "Vector3";
// Where a wrapper (GameObject, Transform, Rigidbody...) keeps which object it stands for: the
// object's slot and its serial (see ObjectSerial), and, on a GameObject, its name and tag.
inline constexpr const char* kIdVariable = "@id";
inline constexpr const char* kSerialVariable = "@serial";
inline constexpr const char* kNameVariable = "@name";
inline constexpr const char* kTagVariable = "@tag";

ScriptContext& context(mrb_state* mrb);
Scene& scene(mrb_state* mrb);

// A part of the engine a native cannot do without. A context that has none (a script run outside
// a game) gets a RuntimeError saying `unavailable`.
template <typename Part>
Part& required(mrb_state* mrb, Part* part, const char* unavailable) {
  if (part == nullptr) mrb_raise(mrb, E_RUNTIME_ERROR, unavailable);
  return *part;
}

// The handle a wrapper keeps (its @id and @serial); raises for a value that is no wrapper. Its
// object is scene(mrb).resolve(handle): nothing once destroyed, or once the scene was switched.
ObjectHandle wrapped_handle(mrb_state* mrb, mrb_value self);
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
// What a name stands for; an ArgumentError (`unknown`, which says the name where its %s is) for a
// name that stands for nothing.
template <typename Found>
Found known(mrb_state* mrb, const std::optional<Found>& found, const char* unknown, const std::string& name) {
  if (!found) mrb_raisef(mrb, E_ARGUMENT_ERROR, unknown, name.c_str());
  return *found;
}
// A number a script handed a native, when finite: an ArgumentError naming the native otherwise.
mrb_float finite(mrb_state* mrb, mrb_float value);
// A finite number within a range of its own: an ArgumentError saying "<what> must be ..., not
// <value>" otherwise.
mrb_float at_least(mrb_state* mrb, mrb_float value, mrb_float low, const char* what);
mrb_float more_than(mrb_state* mrb, mrb_float value, mrb_float low, const char* what);
mrb_float from_to(mrb_state* mrb, mrb_float value, mrb_float low, mrb_float high, const char* what);

// The single argument of a native method: any value, a finite number, true or false, or a name
// (a Symbol or a String).
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
