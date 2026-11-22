// The Environment module's natives: the scene's look (its sky, fog, light and effects) changed while
// the game runs. They write the properties of the scene's environment object (environment = true),
// which the renderer reads every frame, so a change shows on the next frame drawn.

#include <mruby.h>
#include <mruby/string.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <string>

#include "scene/property.hpp"
#include "scene/props.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using namespace binding;
namespace env = props::environment;

// What a setting holds.
enum class Kind { Color, Number, Text, Flag };

struct Setting {
  const char* key;
  Kind kind;
};

// Every setting of an environment object (see render/environment.hpp).
constexpr Setting kSettings[] = {
    {env::kSkyTop, Kind::Color},
    {env::kSkyHorizon, Kind::Color},
    {env::kGroundColor, Kind::Color},
    {env::kFogColor, Kind::Color},
    {env::kFogDensity, Kind::Number},
    {env::kFogHeight, Kind::Number},
    {env::kFogFalloff, Kind::Number},
    {env::kFogSunScatter, Kind::Number},
    {env::kHaze, Kind::Number},
    {env::kAmbient, Kind::Number},
    {env::kExposure, Kind::Number},
    {env::kBloom, Kind::Number},
    {env::kVignette, Kind::Number},
    {env::kSunGlow, Kind::Number},
    {env::kWind, Kind::Number},
    {env::kShadows, Kind::Number},
    {env::kAmbientOcclusion, Kind::Number},
    {env::kContrast, Kind::Number},
    {env::kSaturation, Kind::Number},
    {env::kGrain, Kind::Number},
    {env::kAberration, Kind::Number},
    {env::kSkyImage, Kind::Text},
    {env::kSkyRotation, Kind::Number},
    {env::kSkyIntensity, Kind::Number},
    {env::kMsaa, Kind::Number},
    {env::kShadowCascades, Kind::Number},
    {env::kShadowDistance, Kind::Number},
    {env::kShadowSoftness, Kind::Number},
    {env::kLocalShadows, Kind::Number},
    {env::kAntialias, Kind::Text},
    {env::kGlowLimit, Kind::Number},
    {env::kAutoExposure, Kind::Flag},
    {env::kExposureMinEv, Kind::Number},
    {env::kExposureMaxEv, Kind::Number},
    {env::kExposureSpeedUp, Kind::Number},
    {env::kExposureSpeedDown, Kind::Number},
    {env::kExposureCompensation, Kind::Number},
    {env::kFoliageTranslucency, Kind::Number},
};

const Setting& setting_named(mrb_state* mrb, const std::string& key) {
  const auto* found = std::find_if(std::begin(kSettings), std::end(kSettings),
                                   [&](const Setting& setting) { return key == setting.key; });
  if (found == std::end(kSettings)) mrb_raisef(mrb, E_ARGUMENT_ERROR, "Environment has no setting %s", key.c_str());
  return *found;
}

// Whether an object is one whose properties describe the environment (scene_environment() takes
// the first, until it is gone).
bool is_environment(const GameObject& object) { return get_bool(object.props, props::kEnvironment).value_or(false); }

// The scene's environment object, as scene_environment() finds it, or nullptr.
GameObject* environment_object(mrb_state* mrb) {
  Scene& objects = scene(mrb);
  for (const ObjectId id : objects.alive_ids()) {
    if (is_environment(objects.at(id))) return &objects.at(id);
  }
  return nullptr;
}

GameObject& required_environment(mrb_state* mrb) {
  GameObject* found = environment_object(mrb);
  if (found == nullptr) mrb_raise(mrb, E_RUNTIME_ERROR, "the scene has no environment object (environment = true)");
  return *found;
}

// A name given as a String or a Symbol.
std::string text_of(mrb_state* mrb, mrb_value value, const char* key) {
  const bool named = mrb_string_p(value) || mrb_symbol_p(value);
  if (!named) mrb_raisef(mrb, E_ARGUMENT_ERROR, "Environment %s takes a String or a Symbol, not %v", key, value);
  return to_name(mrb, value);
}

std::string color_of(mrb_state* mrb, mrb_value value, const char* key) {
  std::string text = text_of(mrb, value, key);
  if (!parse_hex_color(text)) mrb_raisef(mrb, E_ARGUMENT_ERROR, "Environment %s takes \"#rrggbb\", not %v", key, value);
  return text;
}

bool flag_of(mrb_state* mrb, mrb_value value, const char* key) {
  const bool flag = mrb_true_p(value) || mrb_false_p(value);
  if (!flag) mrb_raisef(mrb, E_ARGUMENT_ERROR, "Environment %s takes true or false, not %v", key, value);
  return mrb_true_p(value);
}

double number_of(mrb_state* mrb, mrb_value value, const char* key) {
  const mrb_float number = mrb_as_float(mrb, value);
  if (!std::isfinite(number)) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "Environment %s takes a finite number, not %v", key, value);
  }
  return static_cast<double>(number);
}

PropertyValue value_of(mrb_state* mrb, const Setting& setting, mrb_value value) {
  switch (setting.kind) {
    case Kind::Color:
      return color_of(mrb, value, setting.key);
    case Kind::Text:
      return text_of(mrb, value, setting.key);
    case Kind::Flag:
      return flag_of(mrb, value, setting.key);
    case Kind::Number:
      break;
  }
  return number_of(mrb, value, setting.key);
}

// A setting as its kind shows it (a scene from Blender keeps a flag as 0 or 1).
mrb_value shown(mrb_state* mrb, const Setting& setting, const PropertyMap& props) {
  const PropertyValue& value = props.at(setting.key);
  if (setting.kind == Kind::Flag) return mrb_bool_value(get_bool(props, setting.key).value_or(false));
  if (const auto* flag = std::get_if<bool>(&value)) return mrb_bool_value(*flag);
  if (const auto* number = std::get_if<double>(&value)) return mrb_float_value(mrb, *number);
  return text_value(mrb, std::get<std::string>(value));
}

// __get(key): what the environment object says of a setting, or nil (the default applies).
mrb_value environment_get(mrb_state* mrb, mrb_value) {
  const Setting setting = setting_named(mrb, name_arg(mrb));
  const GameObject* object = environment_object(mrb);
  if (object == nullptr) return mrb_nil_value();
  if (object->props.count(setting.key) == 0) return mrb_nil_value();
  return shown(mrb, setting, object->props);
}

// __set(key, value): changes a setting; nil gives it back to the default.
mrb_value environment_set(mrb_state* mrb, mrb_value) {
  mrb_value key = mrb_nil_value();
  mrb_value value = mrb_nil_value();
  mrb_get_args(mrb, "oo", &key, &value);
  const Setting setting = setting_named(mrb, to_name(mrb, key));
  GameObject& object = required_environment(mrb);
  if (mrb_nil_p(value)) {
    object.props.erase(setting.key);
  } else {
    object.props[setting.key] = value_of(mrb, setting, value);
  }
  return value;
}

mrb_value environment_present_p(mrb_state* mrb, mrb_value) {
  return mrb_bool_value(environment_object(mrb) != nullptr);
}

}  // namespace

void install_environment_bindings(mrb_state* mrb) {
  RClass* environment = mrb_define_module(mrb, "Environment");
  mrb_define_module_function(mrb, environment, "__get", environment_get, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, environment, "__set", environment_set, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, environment, "present?", environment_present_p, MRB_ARGS_NONE());
}

}  // namespace redgem
