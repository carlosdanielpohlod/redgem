// Decals natives (ruby/engine/decals.rb holds the defaults).

#include <mruby.h>

#include "scene/decals.hpp"
#include "script/bindings/binding_helpers.hpp"
#include "script/bindings/bindings.hpp"

namespace redgem {
namespace {

using namespace binding;

constexpr mrb_float kLargestDecal = 100.0;  // meters across or deep: a larger one is a mistake
constexpr mrb_float kDegreesToRadians = static_cast<mrb_float>(kPi) / kDegreesPerHalfTurn;
constexpr std::size_t kPlaceArgs = 10;

DecalSystem& decals(mrb_state* mrb) {
  return required(mrb, context(mrb).decals, "Decals are only available while a game runs");
}

// What Decals.__place is called with, as mruby hands it over.
struct PlaceArgs {
  mrb_value point = mrb_nil_value();
  mrb_value normal = mrb_nil_value();
  mrb_float size = 0.0, depth = 0.0, spin = 0.0;
  const char* image = nullptr;
  mrb_float red = 0.0, green = 0.0, blue = 0.0, alpha = 0.0;
};

PlaceArgs place_args(mrb_state* mrb) {
  PlaceArgs args;
  mrb_get_args(mrb, "oofffzffff", &args.point, &args.normal, &args.size, &args.depth, &args.spin, &args.image,
               &args.red, &args.green, &args.blue, &args.alpha);
  return args;
}

Vec3 normal_of(mrb_state* mrb, mrb_value value) {
  const Vec3 normal = to_vec3(mrb, value);
  if (length(normal) <= kEpsilon) mrb_raise(mrb, E_ARGUMENT_ERROR, "Decals.place: the normal must not be zero");
  return normal;
}

Color color_of(mrb_state* mrb, const PlaceArgs& args) {
  return {channel_from(mrb, args.red, "a decal's color channel"), channel_from(mrb, args.green, "a decal's color channel"),
          channel_from(mrb, args.blue, "a decal's color channel"), channel_from(mrb, args.alpha, "a decal's alpha")};
}

DecalPlacement placement_of(mrb_state* mrb, const PlaceArgs& args) {
  DecalPlacement placement;
  placement.point = to_vec3(mrb, args.point);
  placement.normal = normal_of(mrb, args.normal);
  placement.size = static_cast<float>(from_to(mrb, args.size, 0.0, kLargestDecal, "a decal's size"));
  more_than(mrb, args.size, 0.0, "a decal's size");
  placement.depth = static_cast<float>(from_to(mrb, args.depth, 0.0, kLargestDecal, "a decal's depth"));
  more_than(mrb, args.depth, 0.0, "a decal's depth");
  placement.spin_radians = static_cast<float>(finite(mrb, args.spin) * kDegreesToRadians);
  placement.image = args.image;
  placement.color = color_of(mrb, args);
  return placement;
}

// (A machine that draws nothing, a dedicated server, keeps no marks.)
mrb_value decals_place(mrb_state* mrb, mrb_value) {
  const DecalPlacement placement = placement_of(mrb, place_args(mrb));
  DecalSystem& system = decals(mrb);
  if (!context(mrb).renders_particles) return mrb_false_value();
  const SceneDesc& desc = required(mrb, context(mrb).scene_desc, "Decals are only available while a game runs");
  return mrb_bool_value(paused(mrb, [&] { return system.place(scene(mrb), desc, placement); }));
}

mrb_value decals_count(mrb_state* mrb, mrb_value) {
  return mrb_int_value(mrb, static_cast<mrb_int>(decals(mrb).decals().size()));
}

mrb_value decals_limit(mrb_state* mrb, mrb_value) { return mrb_int_value(mrb, static_cast<mrb_int>(decals(mrb).limit())); }

mrb_value decals_set_limit(mrb_state* mrb, mrb_value) {
  const mrb_float limit = from_to(mrb, float_arg(mrb), static_cast<mrb_float>(kMinDecalLimit),
                                  static_cast<mrb_float>(kMaxDecalLimit), "Decals.limit");
  decals(mrb).set_limit(static_cast<std::size_t>(limit));
  return mrb_int_value(mrb, static_cast<mrb_int>(decals(mrb).limit()));
}

mrb_value decals_clear(mrb_state* mrb, mrb_value) {
  decals(mrb).clear();
  return mrb_nil_value();
}

}  // namespace

void install_decals_bindings(mrb_state* mrb) {
  RClass* decals_module = mrb_define_module(mrb, "Decals");
  mrb_define_module_function(mrb, decals_module, "__place", decals_place, MRB_ARGS_REQ(kPlaceArgs));
  mrb_define_module_function(mrb, decals_module, "count", decals_count, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, decals_module, "limit", decals_limit, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, decals_module, "limit=", decals_set_limit, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, decals_module, "clear", decals_clear, MRB_ARGS_NONE());
}

}  // namespace redgem
