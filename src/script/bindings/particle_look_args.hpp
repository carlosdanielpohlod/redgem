#pragma once

// What Particles.burst says a burst looks like, read from what Ruby hands over and checked as every
// native's numbers are (see binding_helpers.hpp).

#include <mruby.h>

#include "scene/particle_look.hpp"

namespace redgem::binding {

// From an Array of 1 to kMaxLifeKeys values, spread over the life. One size or one alpha alone
// does what particles always did: it holds for half the life, then a size shrinks to
// kEndSizeShare of itself and an alpha fades out. One color holds all life long.
OverLife<float> sizes_over_life(mrb_state* mrb, mrb_value sizes);    // meters
OverLife<float> alphas_over_life(mrb_state* mrb, mrb_value alphas);  // 0 to 255
OverLife<Vec3> colors_over_life(mrb_state* mrb, mrb_value colors);   // [r, g, b], each 0 to 255

// A sprite sheet of `columns` x `rows` pictures, shown at `fps` (0: once over the life).
Flipbook flipbook_of(mrb_state* mrb, mrb_int columns, mrb_int rows, mrb_float fps);

}  // namespace redgem::binding
