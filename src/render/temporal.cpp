#include "render/temporal.hpp"

#include <cmath>

namespace redgem::render_math {
namespace {

constexpr int kAcrossBase = 2;  // the Halton bases: the places across ...
constexpr int kDownBase = 3;    // ... and down
// Each frame's pattern is shifted by the golden ratio's fraction from the last: the shifts come
// back to no value soon, and spread evenly over 0..1.
constexpr double kGoldenFraction = 0.6180339887498949;
constexpr float kNdcAcross = 2.0f;  // normalized device coordinates span 2 across a picture (and down)

double fraction(double value) { return value - std::floor(value); }

// Whether `now`'s field of view differs from `before`'s by more than kCutZoom of it.
bool zoomed(float before, float now) { return std::fabs(now - before) > kCutZoom * before; }

}  // namespace

float halton(int index, int base) {
  float result = 0.0f;
  float fraction_of_base = 1.0f;
  for (int rest = index; rest > 0; rest /= base) {
    fraction_of_base /= static_cast<float>(base);
    result += fraction_of_base * static_cast<float>(rest % base);
  }
  return result;
}

Vector2 jitter_of(std::uint64_t frame) {
  const int index = static_cast<int>(frame % kJitterPhases) + 1;  // (the sequence's 0th is 0: no jitter at all)
  return {halton(index, kAcrossBase) - kHalf, halton(index, kDownBase) - kHalf};
}

// Clip coordinates across (and down) gain the depth row times the shift: after the division by
// w, every point lands the same share of the picture farther, whatever its distance.
Matrix jittered(const Matrix& projection, Vector2 pixels, int width, int height) {
  const float across = kNdcAcross * pixels.x / static_cast<float>(width);
  const float down = kNdcAcross * pixels.y / static_cast<float>(height);
  Matrix moved = projection;
  moved.m0 += across * projection.m3;
  moved.m4 += across * projection.m7;
  moved.m8 += across * projection.m11;
  moved.m12 += across * projection.m15;
  moved.m1 += down * projection.m3;
  moved.m5 += down * projection.m7;
  moved.m9 += down * projection.m11;
  moved.m13 += down * projection.m15;
  return moved;
}

float noise_shift(std::uint64_t frame) {
  return static_cast<float>(fraction(static_cast<double>(frame) * kGoldenFraction));
}

bool resized(const TemporalView& before, const TemporalView& now) {
  return before.width != now.width || before.height != now.height;
}

bool camera_cut(const TemporalView& before, const TemporalView& now) {
  const bool moved_far = length(now.position - before.position) > kCutDistance;
  const bool turned_far = dot(now.forward, before.forward) < kCutTurnCosine;
  return moved_far || turned_far || zoomed(before.fov_y, now.fov_y);
}

bool keeps_history(const TemporalView& before, const TemporalView& now) {
  return !resized(before, now) && before.scene == now.scene && !camera_cut(before, now);
}

}  // namespace redgem::render_math
