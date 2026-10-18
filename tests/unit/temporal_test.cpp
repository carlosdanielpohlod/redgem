// Temporal anti-aliasing: the jitter's Halton places, the view moved by them, the shift of the
// pixels' fine patterns, and when the frames before cannot be used.
#include <doctest/doctest.h>

#include <cmath>
#include <set>

#include "render/temporal.hpp"

#include <raymath.h>  // after raylib.h (through temporal.hpp): it relies on raylib's own types

using namespace redgem;
using namespace redgem::render_math;

namespace {

constexpr int kWidth = 1280;
constexpr int kHeight = 720;
constexpr float kFovY = 60.0f;
constexpr float kNear = 0.1f;
constexpr float kFar = 1000.0f;
constexpr Vector3 kPoint{1.5f, -0.75f, -12.0f};  // in front of a camera at the origin looking down -Z
constexpr Vector2 kShift{0.25f, -0.375f};        // pixels
constexpr float kPixelTolerance = 0.001f;
constexpr std::uint64_t kSomeFrame = 41;
constexpr float kStep = 0.5f;  // meters, or a little turn: within a frame's move
constexpr std::uint64_t kNextScene = 2;

// Where `point` lands on a `kWidth` x `kHeight` picture through `projection`, in pixels.
Vector2 pixel_of(const Matrix& projection, Vector3 point) {
  const Vector4 clip{projection.m0 * point.x + projection.m4 * point.y + projection.m8 * point.z + projection.m12,
                     projection.m1 * point.x + projection.m5 * point.y + projection.m9 * point.z + projection.m13, 0.0f,
                     projection.m3 * point.x + projection.m7 * point.y + projection.m11 * point.z + projection.m15};
  return {(clip.x / clip.w + 1.0f) * kHalf * kWidth, (clip.y / clip.w + 1.0f) * kHalf * kHeight};
}

Matrix perspective() { return MatrixPerspective(kFovY * DEG2RAD, static_cast<double>(kWidth) / kHeight, kNear, kFar); }

TemporalView looking_ahead() { return {kWidth, kHeight, Vec3{0.0f, 1.0f, 0.0f}, Vec3{0.0f, 0.0f, -1.0f}, kFovY, 1}; }

}  // namespace

TEST_CASE("the Halton sequence spreads its numbers over 0..1 by digits reversed") {
  CHECK(halton(1, 2) == doctest::Approx(0.5f));
  CHECK(halton(2, 2) == doctest::Approx(0.25f));
  CHECK(halton(3, 2) == doctest::Approx(0.75f));
  CHECK(halton(1, 3) == doctest::Approx(1.0f / 3.0f));
  CHECK(halton(2, 3) == doctest::Approx(2.0f / 3.0f));
  CHECK(halton(4, 3) == doctest::Approx(4.0f / 9.0f));
}

TEST_CASE("the jitter moves the view by less than a pixel, to a different place each frame of its cycle") {
  std::set<std::pair<float, float>> places;
  for (std::uint64_t frame = 0; frame < kJitterPhases; ++frame) {
    const Vector2 offset = jitter_of(frame);
    CHECK(std::fabs(offset.x) < kHalf);
    CHECK(std::fabs(offset.y) < kHalf);
    places.insert({offset.x, offset.y});
  }
  CHECK(places.size() == static_cast<std::size_t>(kJitterPhases));
  CHECK(jitter_of(kJitterPhases).x == jitter_of(0).x);  // then it comes around again
  CHECK(jitter_of(0).x == doctest::Approx(0.0f));       // halton(1, 2) is a half: the pixel's middle
}

TEST_CASE("a jittered projection moves whatever it shows by the jitter, near or far") {
  const Matrix still = perspective();
  const Matrix moved = jittered(still, kShift, kWidth, kHeight);
  for (const Vector3 point : {kPoint, Vector3Scale(kPoint, 20.0f)}) {
    const Vector2 before = pixel_of(still, point);
    const Vector2 after = pixel_of(moved, point);
    CHECK(after.x - before.x == doctest::Approx(kShift.x).epsilon(kPixelTolerance));
    CHECK(after.y - before.y == doctest::Approx(kShift.y).epsilon(kPixelTolerance));
  }
  const Matrix none = jittered(still, Vector2{0.0f, 0.0f}, kWidth, kHeight);
  CHECK(none.m8 == still.m8);
  CHECK(none.m9 == still.m9);
}

TEST_CASE("the pixels' fine pattern is shifted differently every frame, within 0..1") {
  const float first = noise_shift(kSomeFrame);
  const float next = noise_shift(kSomeFrame + 1);
  CHECK(first >= 0.0f);
  CHECK(first < 1.0f);
  CHECK(next != doctest::Approx(first));
  CHECK(noise_shift(0) == 0.0f);
}

TEST_CASE("the frames before are kept while the camera moves on smoothly") {
  const TemporalView before = looking_ahead();
  TemporalView now = before;
  now.position = before.position + Vec3{kStep, 0.0f, 0.0f};
  now.forward = normalize(Vec3{kStep * kHalf, 0.0f, -1.0f});
  CHECK(keeps_history(before, now));
}

TEST_CASE("the frames before are dropped when the picture changes size or the scene is another") {
  const TemporalView before = looking_ahead();
  TemporalView wider = before;
  wider.width = kWidth * 2;
  CHECK(resized(before, wider));
  CHECK_FALSE(keeps_history(before, wider));
  TemporalView loaded = before;
  loaded.scene = kNextScene;
  CHECK_FALSE(keeps_history(before, loaded));
}

TEST_CASE("the frames before are dropped when the camera cuts: jumps, turns around or zooms") {
  const TemporalView before = looking_ahead();
  TemporalView jumped = before;
  jumped.position = before.position + Vec3{kCutDistance + kStep, 0.0f, 0.0f};
  CHECK(camera_cut(before, jumped));
  TemporalView turned = before;
  turned.forward = Vec3{1.0f, 0.0f, 0.0f};
  CHECK(camera_cut(before, turned));
  TemporalView zoomed = before;
  zoomed.fov_y = kFovY * kHalf;
  CHECK(camera_cut(before, zoomed));
  for (const TemporalView& cut : {jumped, turned, zoomed}) CHECK_FALSE(keeps_history(before, cut));
}
