#include "script/bindings/particle_look_args.hpp"

#include <mruby/array.h>

#include "script/bindings/binding_helpers.hpp"

namespace redgem::binding {
namespace {

constexpr mrb_float kLargestSheetSide = 64.0;  // pictures across or down a sprite sheet
constexpr mrb_int kColorChannels = 3;

// The keys of a value over life, from a list of one to kMaxLifeKeys values, each read by `read`.
template <typename Value, typename Read>
OverLife<Value> keys_of(mrb_state* mrb, mrb_value list, const char* how_many, Read read) {
  OverLife<Value> over_life;
  const auto count = static_cast<mrb_float>(RARRAY_LEN(list));
  over_life.count = static_cast<int>(from_to(mrb, count, 1.0, static_cast<mrb_float>(kMaxLifeKeys), how_many));
  for (int i = 0; i < over_life.count; ++i) {
    over_life.keys[static_cast<std::size_t>(i)] = read(mrb_ary_entry(list, i));
  }
  return over_life;
}

// A color's channel or an alpha, as scripts give them: 0 to 255.
float channel_of(mrb_state* mrb, mrb_value value, const char* what) {
  return channel_from(mrb, mrb_as_float(mrb, value), what);
}

// A color's channels as 0..1, from [r, g, b] with each 0 to 255.
Vec3 color_of(mrb_state* mrb, mrb_value color) {
  const bool triple = mrb_array_p(color) && RARRAY_LEN(color) == kColorChannels;
  if (!triple) mrb_raise(mrb, E_TYPE_ERROR, "a burst's color: expected [r, g, b]");
  float channels[kColorChannels] = {};
  for (mrb_int i = 0; i < kColorChannels; ++i)
    channels[i] = channel_of(mrb, mrb_ary_entry(color, i), "a color channel");
  return vec3_from(channels);
}

int sheet_side(mrb_state* mrb, mrb_int side, const char* what) {
  return static_cast<int>(from_to(mrb, static_cast<mrb_float>(side), 1.0, kLargestSheetSide, what));
}

}  // namespace

OverLife<float> sizes_over_life(mrb_state* mrb, mrb_value sizes) {
  const OverLife<float> over_life =
      keys_of<float>(mrb, sizes, "how many sizes a burst goes through", [mrb](mrb_value size) {
        return static_cast<float>(at_least(mrb, mrb_as_float(mrb, size), 0.0, "a burst's size"));
      });
  if (over_life.count > 1) return over_life;
  return held_then(over_life.keys[0], over_life.keys[0] * kEndSizeShare);
}

OverLife<float> alphas_over_life(mrb_state* mrb, mrb_value alphas) {
  const OverLife<float> over_life =
      keys_of<float>(mrb, alphas, "how many alphas a burst goes through",
                     [mrb](mrb_value alpha) { return channel_of(mrb, alpha, "a burst's alpha"); });
  if (over_life.count > 1) return over_life;
  return held_then(over_life.keys[0], 0.0f);
}

OverLife<Vec3> colors_over_life(mrb_state* mrb, mrb_value colors) {
  return keys_of<Vec3>(mrb, colors, "how many colors a burst goes through",
                       [mrb](mrb_value color) { return color_of(mrb, color); });
}

Flipbook flipbook_of(mrb_state* mrb, mrb_int columns, mrb_int rows, mrb_float fps) {
  Flipbook book;
  book.columns = sheet_side(mrb, columns, "a sprite sheet's columns");
  book.rows = sheet_side(mrb, rows, "a sprite sheet's rows");
  book.fps = static_cast<float>(at_least(mrb, fps, 0.0, "a flipbook's fps"));
  return book;
}

}  // namespace redgem::binding
