#pragma once

// Temporal anti-aliasing (Environment::temporal_antialias): each frame the view is moved by less
// than a pixel, a different amount every frame, and the pictures of the frames before, moved to
// where each pixel was then, are mixed into this one's. Edges, cut-out leaves and the fine
// patterns of fading copies come out as smooth as many samples a pixel would make them. Here:
// those amounts, the view moved by them, and when the frames before cannot be used. No window and
// no graphics card needed, so it can be tested.

#include <raylib.h>

#include <cstdint>

#include "core/math.hpp"

namespace redgem::render_math {

// The view comes back to the same place every this many frames.
inline constexpr int kJitterPhases = 8;
// Where the view is moved to in phase `frame` (counted on past kJitterPhases), in pixels across
// and up, each from -0.5 to 0.5: the Halton sequence of bases 2 and 3, which spreads the
// places evenly over a pixel however many of them are taken.
Vector2 jitter_of(std::uint64_t frame);
// The `index`th number (from 1) of the Halton sequence of `base`: 0..1.
float halton(int index, int base);
// `projection` moved by `pixels` on a picture `width` x `height`: whatever it shows lands that
// many pixels across and up from where it would.
Matrix jittered(const Matrix& projection, Vector2 pixels, int width, int height);
// What a fine pattern that depends on the pixel (fading copies, dithered see-through surfaces) is
// shifted by in `frame`, so that over several frames each pixel takes every value: 0..1.
float noise_shift(std::uint64_t frame);

// What a frame was seen from and drawn into, as the frames after it judge whether they can use
// its picture.
struct TemporalView {
  int width = 0;  // pixels of the picture
  int height = 0;
  Vec3 position;  // of the camera
  Vec3 forward;   // unit length
  float fov_y = 0.0f;
  std::uint64_t scene = 0;  // the scene's upload number: a new scene shares nothing with the last
};

// A camera that moved farther than this in a frame cut to another place.
inline constexpr float kCutDistance = 5.0f;  // meters
// Nor one that turned more than this: the cosine of 45 degrees.
inline constexpr float kCutTurnCosine = 0.7071f;
// Nor one whose field of view changed by more than this share.
inline constexpr float kCutZoom = 0.25f;

bool resized(const TemporalView& before, const TemporalView& now);
bool camera_cut(const TemporalView& before, const TemporalView& now);
// Whether the frame `now` can use the picture of the one `before` it: the same picture size and
// scene, and the camera did not cut.
bool keeps_history(const TemporalView& before, const TemporalView& now);

}  // namespace redgem::render_math
