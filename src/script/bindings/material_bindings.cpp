// The Material module's natives: the glow of the scene's materials, by name, changed while the game
// runs (lit windows at night). What they set is laid over the scene's file by the renderer, every
// frame, so it holds across copies, spawned prefabs and new scenes of materials of that name.

#include <mruby.h>

#include <string>

#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using namespace binding;

constexpr mrb_float kNoLight = 0.0;

EmissionOverrides& overrides(mrb_state* mrb) { return context(mrb).material_emission; }

// A share of emitted light a script gives: a finite number from 0 up.
float light_from(mrb_state* mrb, mrb_float value) {
  return static_cast<float>(at_least(mrb, value, kNoLight, "Material.set_emission's light"));
}

// __set_emission(name, red, green, blue): linear light, already multiplied by its strength.
mrb_value material_set_emission(mrb_state* mrb, mrb_value) {
  mrb_value name = mrb_nil_value();
  mrb_float red = 0.0;
  mrb_float green = 0.0;
  mrb_float blue = 0.0;
  mrb_get_args(mrb, "offf", &name, &red, &green, &blue);
  // (The light is checked before the name goes in: a refused light changes nothing.)
  const Vec3 light{light_from(mrb, red), light_from(mrb, green), light_from(mrb, blue)};
  overrides(mrb)[to_name(mrb, name)] = light;
  return mrb_nil_value();
}

// __reset_emission(name): the materials of that name glow as the scene's file says again.
mrb_value material_reset_emission(mrb_state* mrb, mrb_value) {
  overrides(mrb).erase(name_arg(mrb));
  return mrb_nil_value();
}

}  // namespace

void install_material_bindings(mrb_state* mrb) {
  RClass* material = mrb_define_module(mrb, "Material");
  mrb_define_module_function(mrb, material, "__set_emission", material_set_emission, MRB_ARGS_REQ(4));
  mrb_define_module_function(mrb, material, "__reset_emission", material_reset_emission, MRB_ARGS_REQ(1));
}

}  // namespace redgem
