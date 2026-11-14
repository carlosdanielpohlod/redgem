#pragma once

// Auto exposure (eye adaptation): the picture is brightened in the dark and darkened in bright
// light, slowly, as eyes adapt. Each frame the graphics card measures the scene's average
// brightness (the mean of the log2 of each pixel's luminance, so that a few very bright pixels do
// not darken everything), and the exposure moves toward what makes that average a middle grey.
// Brightness is counted in EV, steps of a doubling: EV 0 is a scene whose average is middle grey,
// EV 1 one twice as bright. The settings and the math the shaders follow are here; no window and
// no graphics card needed, so they can be tested.

#include <cstdint>
#include <optional>

namespace redgem {

// What a scene asks of auto exposure (environment properties, see environment.hpp).
inline constexpr float kDarkestExposureEv = -4.0f;   // EV: darker scenes are left dark
inline constexpr float kBrightestExposureEv = 4.0f;  // EV: brighter scenes are left bright
// The furthest any EV setting (range, compensation) goes either way: a moonless night to the sun.
inline constexpr float kWidestExposureEv = 16.0f;
// Per second: how fast the exposure closes its gap to the scene's brightness. When the scene got
// brighter (EV up) eyes adapt fast; when it got darker (EV down), slowly.
inline constexpr float kExposureSpeedUp = 3.0f;
inline constexpr float kExposureSpeedDown = 1.0f;
inline constexpr float kNoExposureSpeed = 0.0f;         // it never adapts
inline constexpr float kFastestExposureSpeed = 100.0f;  // per second: past this it is there at once anyway

struct AutoExposure {
  bool enabled = false;                   // off: the environment's `exposure` alone, as before
  float min_ev = kDarkestExposureEv;      // the darkest scene brightness it adapts to
  float max_ev = kBrightestExposureEv;    // the brightest
  float speed_up = kExposureSpeedUp;      // per second, when the scene got brighter (0: it never adapts)
  float speed_down = kExposureSpeedDown;  // per second, when the scene got darker
  float compensation = 0.0f;              // EV the adapted picture is brightened by (negative: darkened)
};

// What a game's camera changes of the scene's settings (Camera.auto_exposure and the others in
// Ruby): what it leaves empty follows the scene.
struct AutoExposureOverrides {
  std::optional<bool> enabled;
  std::optional<float> min_ev;
  std::optional<float> max_ev;
  std::optional<float> speed_up;
  std::optional<float> speed_down;
  std::optional<float> compensation;
};

// The scene's settings with what the camera changes. The brightest is never below the darkest.
AutoExposure with_overrides(AutoExposure settings, const AutoExposureOverrides& overrides);

namespace render_math {

// log2 of middle grey: the average a picture is exposed for.
inline constexpr float kMiddleGreyLog2 = -2.473931f;  // log2(0.18)
// The longest step the exposure adapts by in one frame, in seconds: a long stall (loading) must
// not jump it.
inline constexpr float kLongestAdaptStep = 0.25f;
// Texels along each side of the small target the picture's brightness is measured into, cell by
// cell, before it is averaged.
inline constexpr int kLuminanceSide = 32;

// The brightness of a scene whose pixels' mean log2 luminance is `mean_log2`, held to the settings'
// range: what the exposure adapts toward.
float target_ev(float mean_log2, const AutoExposure& settings);
// The brightness the exposure is adapted to after `seconds` more (held to kLongestAdaptStep),
// from `current` toward `target`: it closes the same share of the gap every moment, at the
// settings' speed up when the scene got brighter and down when it got darker.
float adapted_ev(float current, float target, const AutoExposure& settings, float seconds);
// What the picture's light is multiplied by, adapted to `ev`: middle grey at EV 0 stays as it is,
// and `compensation` brightens it by that many doublings.
float exposure_scale(float ev, float compensation);

// What the exposure was adapted from, frame to frame: whether it was at all, through which camera
// object (its serial), in which uploaded scene, and when (the renderer's clock, in seconds).
struct ExposureHistory {
  bool adapted = false;
  std::uint64_t viewer = 0;
  std::uint64_t scene = 0;
  float time = 0.0f;
};
// Whether this frame's exposure is set straight to the scene's brightness instead of adapting to
// it: nothing was adapted yet, or the picture is of another camera or another scene (a cut).
bool starts_anew(const ExposureHistory& history, std::uint64_t viewer, std::uint64_t scene);
// The seconds the exposure adapts by this frame: since the last one, never below none (a clock set
// back) nor above kLongestAdaptStep.
float adapt_seconds(const ExposureHistory& history, float now);

}  // namespace render_math
}  // namespace redgem
