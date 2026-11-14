// The Water module: where the scene's water is, for scripts (a swimmer, a splash, a fish that
// stays under). Heights are the undisplaced surface; waves are not counted.

#include <mruby.h>

#include <optional>
#include <vector>

#include "scene/water_volume.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using namespace binding;

// The running scene's water surfaces; none outside a running game.
const std::vector<WaterSurface>& surfaces(mrb_state* mrb) {
  static const std::vector<WaterSurface> kNone;
  const auto* water = context(mrb).water;
  return water != nullptr ? *water : kNone;
}

mrb_value float_or_nil(mrb_state* mrb, const std::optional<float>& value) {
  return value ? mrb_float_value(mrb, *value) : mrb_nil_value();
}

// height_at(x, z): the surface's height over that column, or nil where there is no water.
mrb_value water_height_at(mrb_state* mrb, mrb_value) {
  mrb_float x = 0.0;
  mrb_float z = 0.0;
  mrb_get_args(mrb, "ff", &x, &z);
  const auto column =
      water_column_at(surfaces(mrb), static_cast<float>(finite(mrb, x)), static_cast<float>(finite(mrb, z)));
  return column ? mrb_float_value(mrb, column->height) : mrb_nil_value();
}

// depth_at(point): how far under the surface of its column a point is (negative above), or nil.
mrb_value water_depth_at(mrb_state* mrb, mrb_value) {
  return float_or_nil(mrb, water_depth(surfaces(mrb), to_vec3(mrb, single_arg(mrb))));
}

// under?(point): whether a point is below the water surface.
mrb_value water_under_p(mrb_state* mrb, mrb_value) {
  return mrb_bool_value(water_under(surfaces(mrb), to_vec3(mrb, single_arg(mrb))));
}

}  // namespace

void install_water_bindings(mrb_state* mrb) {
  RClass* water = mrb_define_module(mrb, "Water");
  mrb_define_module_function(mrb, water, "height_at", water_height_at, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, water, "depth_at", water_depth_at, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, water, "under?", water_under_p, MRB_ARGS_REQ(1));
}

}  // namespace redgem
