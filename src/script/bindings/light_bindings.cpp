// Light natives: how bright a light shines and in what color, changed while the game runs (a fire
// that flickers, a blast that fades). The renderer reads every light anew each frame.

#include <mruby.h>
#include <mruby/array.h>

#include <cmath>

#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using namespace binding;

// Scripts give colors as the interface does: red, green and blue from 0 to 255, as color pickers
// show them (sRGB). Lights shine in linear light.
constexpr mrb_float kMaxChannel = 255.0;
constexpr double kSrgbGamma = 2.2;
constexpr mrb_int kChannels = 3;

float linear_from_channel(mrb_state* mrb, mrb_int channel) {
  const mrb_float shown = from_to(mrb, static_cast<mrb_float>(channel), 0.0, kMaxChannel, "a light's color channel");
  return static_cast<float>(std::pow(shown / kMaxChannel, kSrgbGamma));
}

mrb_value channel_shown(mrb_state* mrb, float linear) {
  return mrb_int_value(mrb, static_cast<mrb_int>(std::lround(std::pow(linear, 1.0 / kSrgbGamma) * kMaxChannel)));
}

// The light of the object a Light stands for.
LightDesc& light_of(mrb_state* mrb, mrb_value self) {
  GameObject& object = alive_object(mrb, self);
  if (!object.light) mrb_raisef(mrb, E_RUNTIME_ERROR, "game object %s has no light", object.name.c_str());
  return *object.light;
}

mrb_value game_object_has_light(mrb_state* mrb, mrb_value self) {
  return mrb_bool_value(alive_object(mrb, self).light.has_value());
}

mrb_value light_intensity(mrb_state* mrb, mrb_value self) {
  return mrb_float_value(mrb, light_of(mrb, self).intensity);
}

mrb_value light_set_intensity(mrb_state* mrb, mrb_value self) {
  const mrb_float intensity = at_least(mrb, float_arg(mrb), 0.0, "a light's intensity");
  light_of(mrb, self).intensity = static_cast<float>(intensity);
  return mrb_float_value(mrb, intensity);
}

mrb_value light_color(mrb_state* mrb, mrb_value self) {
  const Color& color = light_of(mrb, self).color;
  mrb_value channels = mrb_ary_new_capa(mrb, kChannels);
  for (const float channel : {color.r, color.g, color.b}) mrb_ary_push(mrb, channels, channel_shown(mrb, channel));
  return channels;
}

mrb_value light_set_color(mrb_state* mrb, mrb_value self) {
  mrb_int r = 0, g = 0, b = 0;
  mrb_get_args(mrb, "iii", &r, &g, &b);
  const Color color{linear_from_channel(mrb, r), linear_from_channel(mrb, g), linear_from_channel(mrb, b)};
  light_of(mrb, self).color = color;
  return mrb_nil_value();
}

}  // namespace

void install_light_bindings(mrb_state* mrb) {
  RClass* game_object = mrb_class_get(mrb, kGameObjectClass);
  mrb_define_method(mrb, game_object, "__has_light?", game_object_has_light, MRB_ARGS_NONE());
  RClass* light = mrb_define_class(mrb, "Light", mrb->object_class);
  mrb_define_method(mrb, light, "intensity", light_intensity, MRB_ARGS_NONE());
  mrb_define_method(mrb, light, "intensity=", light_set_intensity, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, light, "color", light_color, MRB_ARGS_NONE());
  mrb_define_method(mrb, light, "__set_color", light_set_color, MRB_ARGS_REQ(3));
}

}  // namespace redgem
