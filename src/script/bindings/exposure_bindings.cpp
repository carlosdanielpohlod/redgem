// Camera natives for auto exposure: what the game changes of the scene's (see
// render/auto_exposure.hpp). Each setting is nil until the game sets it, and nil again gives it back
// to the scene's environment.

#include <mruby.h>
#include <mruby/array.h>

#include <optional>
#include <utility>

#include "render/auto_exposure.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using namespace binding;

constexpr mrb_int kPairParts = 2;  // a range is [darkest, brightest], speeds [up, down]
constexpr mrb_int kFirst = 0;
constexpr mrb_int kSecond = 1;
constexpr double kNoSpeed = 0.0;
constexpr double kFastestSpeed = 100.0;  // per second: past this it is there at once anyway

AutoExposureOverrides& overrides(mrb_state* mrb) { return context(mrb).auto_exposure; }

mrb_value float_or_nil(mrb_state* mrb, const std::optional<float>& value) {
  return value ? mrb_float_value(mrb, *value) : mrb_nil_value();
}

// Two settings as [first, second], or nil when the game set neither.
mrb_value pair_or_nil(mrb_state* mrb, const std::optional<float>& first, const std::optional<float>& second) {
  if (!first) return mrb_nil_value();
  mrb_value both = mrb_ary_new_capa(mrb, kPairParts);
  mrb_ary_push(mrb, both, mrb_float_value(mrb, *first));
  mrb_ary_push(mrb, both, float_or_nil(mrb, second));
  return both;
}

// An EV a script gives: a number within kWidestExposureEv either way.
float ev_from(mrb_state* mrb, mrb_value value, const char* what) {
  return static_cast<float>(from_to(mrb, mrb_as_float(mrb, value), -kWidestExposureEv, kWidestExposureEv, what));
}

float speed_from(mrb_state* mrb, mrb_value value, const char* what) {
  return static_cast<float>(from_to(mrb, mrb_as_float(mrb, value), kNoSpeed, kFastestSpeed, what));
}

using Pair = std::optional<std::pair<float, float>>;

// The two numbers of [a, b], each read by `read`, or nothing for nil: an ArgumentError for anything else.
template <typename Read>
Pair pair_from(mrb_state* mrb, mrb_value value, const char* what, Read read) {
  if (mrb_nil_p(value)) return std::nullopt;
  const bool a_pair = mrb_array_p(value) && RARRAY_LEN(value) == kPairParts;
  if (!a_pair) mrb_raisef(mrb, E_ARGUMENT_ERROR, "%s takes [a, b] or nil, not %v", what, value);
  return std::make_pair(read(mrb, mrb_ary_ref(mrb, value, kFirst), what),
                        read(mrb, mrb_ary_ref(mrb, value, kSecond), what));
}

std::optional<float> first_of(const Pair& pair) { return pair ? std::optional<float>(pair->first) : std::nullopt; }
std::optional<float> second_of(const Pair& pair) { return pair ? std::optional<float>(pair->second) : std::nullopt; }

// Whether a range's brightest end is below its darkest.
bool inverted(const Pair& range) { return range && range->second < range->first; }

mrb_value camera_auto_exposure(mrb_state* mrb, mrb_value) {
  const std::optional<bool>& enabled = overrides(mrb).enabled;
  return enabled ? mrb_bool_value(*enabled) : mrb_nil_value();
}

// Whether a value is true or false.
bool is_boolean(mrb_value value) { return mrb_true_p(value) || mrb_false_p(value); }

mrb_value camera_set_auto_exposure(mrb_state* mrb, mrb_value) {
  const mrb_value value = single_arg(mrb);
  const bool accepted = mrb_nil_p(value) || is_boolean(value);
  if (!accepted) mrb_raisef(mrb, E_ARGUMENT_ERROR, "Camera.auto_exposure takes true, false or nil, not %v", value);
  overrides(mrb).enabled = mrb_nil_p(value) ? std::nullopt : std::optional<bool>(mrb_true_p(value));
  return value;
}

mrb_value camera_exposure_compensation(mrb_state* mrb, mrb_value) {
  return float_or_nil(mrb, overrides(mrb).compensation);
}

mrb_value camera_set_exposure_compensation(mrb_state* mrb, mrb_value) {
  const mrb_value value = single_arg(mrb);
  const char* what = "Camera.exposure_compensation";
  overrides(mrb).compensation = mrb_nil_p(value) ? std::nullopt : std::optional<float>(ev_from(mrb, value, what));
  return value;
}

mrb_value camera_exposure_range(mrb_state* mrb, mrb_value) {
  return pair_or_nil(mrb, overrides(mrb).min_ev, overrides(mrb).max_ev);
}

mrb_value camera_set_exposure_range(mrb_state* mrb, mrb_value) {
  const mrb_value value = single_arg(mrb);
  const Pair range = pair_from(mrb, value, "Camera.exposure_range", ev_from);
  if (inverted(range)) mrb_raise(mrb, E_ARGUMENT_ERROR, "Camera.exposure_range: its brightest is below its darkest");
  overrides(mrb).min_ev = first_of(range);
  overrides(mrb).max_ev = second_of(range);
  return value;
}

mrb_value camera_exposure_speed(mrb_state* mrb, mrb_value) {
  return pair_or_nil(mrb, overrides(mrb).speed_up, overrides(mrb).speed_down);
}

mrb_value camera_set_exposure_speed(mrb_state* mrb, mrb_value) {
  const mrb_value value = single_arg(mrb);
  const Pair speeds = pair_from(mrb, value, "Camera.exposure_speed", speed_from);
  overrides(mrb).speed_up = first_of(speeds);
  overrides(mrb).speed_down = second_of(speeds);
  return value;
}

}  // namespace

void install_exposure_bindings(mrb_state* mrb) {
  RClass* camera = mrb_define_module(mrb, "Camera");
  mrb_define_module_function(mrb, camera, "auto_exposure", camera_auto_exposure, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, camera, "auto_exposure=", camera_set_auto_exposure, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, camera, "exposure_compensation", camera_exposure_compensation, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, camera, "exposure_compensation=", camera_set_exposure_compensation, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, camera, "exposure_range", camera_exposure_range, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, camera, "exposure_range=", camera_set_exposure_range, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, camera, "exposure_speed", camera_exposure_speed, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, camera, "exposure_speed=", camera_set_exposure_speed, MRB_ARGS_REQ(1));
}

}  // namespace redgem
