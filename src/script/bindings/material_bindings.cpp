// The Material module's natives: the glow of the scene's materials, by name, changed while the game
// runs (lit windows at night). What they set is laid over the scene's file by the renderer, every
// frame, so it holds across copies, spawned prefabs and new scenes of materials of that name.

#include <mruby.h>

#include <string>

#include "scene/color_space.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using namespace binding;

constexpr mrb_float kNoGlow = 0.0;
constexpr mrb_float kBrightestGlow = 1000.0;  // times the color: far past what bloom shows, still finite

EmissionOverrides& overrides(mrb_state* mrb) { return context(mrb).material_emission; }

// A channel of the color a script gives (0 to 255, as color pickers show it), in linear light.
float linear_channel(mrb_state* mrb, mrb_float channel) {
  return linear_from_srgb(channel_from(mrb, channel, "Material.set_emission's color channel"));
}

// __set_emission(name, red, green, blue, strength): the color from 0 to 255, times the strength.
// (Everything is checked before the name goes in: a refused glow changes nothing.)
mrb_value material_set_emission(mrb_state* mrb, mrb_value) {
  mrb_value name = mrb_nil_value();
  mrb_float red = 0.0;
  mrb_float green = 0.0;
  mrb_float blue = 0.0;
  mrb_float strength = 0.0;
  mrb_get_args(mrb, "offff", &name, &red, &green, &blue, &strength);
  const auto times =
      static_cast<float>(from_to(mrb, strength, kNoGlow, kBrightestGlow, "Material.set_emission's strength"));
  const Vec3 light = Vec3{linear_channel(mrb, red), linear_channel(mrb, green), linear_channel(mrb, blue)} * times;
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
  mrb_define_module_function(mrb, material, "__set_emission", material_set_emission, MRB_ARGS_REQ(5));
  mrb_define_module_function(mrb, material, "__reset_emission", material_reset_emission, MRB_ARGS_REQ(1));
}

}  // namespace redgem
