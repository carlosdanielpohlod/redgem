// Auto exposure's settings and the math its shaders follow (see render/auto_exposure.hpp): what a
// scene's brightness asks for, how the exposure adapts to it over time, and the settings a scene
// and a game's camera give it.
#include <doctest/doctest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

#include "render/auto_exposure.hpp"
#include "render/environment.hpp"

using namespace redgem;
using namespace redgem::render_math;

namespace {

constexpr float kFrame = 1.0f / 60.0f;  // seconds
constexpr float kLongWhile = 1000.0f;   // seconds: far past any adaptation
constexpr float kDarkScene = -3.0f;     // EV
constexpr float kBrightScene = 3.0f;    // EV
constexpr std::uint64_t kCamera = 7;    // a camera object's serial
constexpr std::uint64_t kOtherCamera = 8;
constexpr std::uint64_t kScene = 2;  // scenes uploaded so far

// The mean log2 luminance of a scene `ev` doublings brighter than middle grey.
float mean_log2_of(float ev) { return kMiddleGreyLog2 + ev; }

// Where the exposure is after `frames` frames of `seconds` each, from `from` toward `target`.
float after(float from, float target, const AutoExposure& settings, int frames, float seconds = kFrame) {
  float ev = from;
  for (int i = 0; i < frames; ++i) ev = adapted_ev(ev, target, settings, seconds);
  return ev;
}

}  // namespace

TEST_CASE("a scene whose average is middle grey asks for no change, brighter ones for less light") {
  const AutoExposure settings;
  CHECK(target_ev(kMiddleGreyLog2, settings) == doctest::Approx(0.0f));
  CHECK(target_ev(mean_log2_of(2.0f), settings) == doctest::Approx(2.0f));
  CHECK(exposure_scale(0.0f, 0.0f) == doctest::Approx(1.0f));
  CHECK(exposure_scale(1.0f, 0.0f) == doctest::Approx(0.5f));   // twice as bright: half the light
  CHECK(exposure_scale(-2.0f, 0.0f) == doctest::Approx(4.0f));  // four times as dark: four times the light
  CHECK(exposure_scale(0.0f, 1.0f) == doctest::Approx(2.0f));   // compensation brightens by doublings
}

TEST_CASE("the brightness adapted to stays within the scene's range, and a broken measure asks for none") {
  AutoExposure settings;
  settings.min_ev = -1.0f;
  settings.max_ev = 1.5f;
  CHECK(target_ev(mean_log2_of(kBrightScene), settings) == doctest::Approx(settings.max_ev));
  CHECK(target_ev(mean_log2_of(kDarkScene), settings) == doctest::Approx(settings.min_ev));
  CHECK(target_ev(std::numeric_limits<float>::quiet_NaN(), settings) == doctest::Approx(0.0f));
  CHECK(target_ev(-std::numeric_limits<float>::infinity(), settings) == doctest::Approx(0.0f));
  settings.max_ev = -2.0f;  // below the darkest: it is the darkest
  CHECK(target_ev(mean_log2_of(kBrightScene), settings) == doctest::Approx(settings.min_ev));
}

TEST_CASE("the exposure adapts smoothly, faster to light than to the dark, and never overshoots") {
  const AutoExposure settings;                                                     // up 3 a second, down 1
  CHECK(adapted_ev(0.0f, kBrightScene, settings, 0.0f) == doctest::Approx(0.0f));  // no time, no change
  const float brighter = after(0.0f, kBrightScene, settings, 1);
  CHECK(brighter > 0.0f);
  CHECK(brighter < kBrightScene);
  const float darker = after(0.0f, -kBrightScene, settings, 1);
  CHECK(-darker < brighter);  // the same gap darker closes slower
  const float half_second = after(0.0f, kBrightScene, settings, 30);
  CHECK(half_second <= kBrightScene);
  CHECK(half_second == doctest::Approx(kBrightScene * (1.0f - std::exp(-settings.speed_up * 0.5f))).epsilon(0.01));
  CHECK(after(0.0f, kBrightScene, settings, 600) == doctest::Approx(kBrightScene));  // settled in the end
}

TEST_CASE("how far the exposure adapts depends on time, not on how many frames it is cut into") {
  const AutoExposure settings;
  const float sixty = after(0.0f, kDarkScene, settings, 60, kFrame);
  const float thirty = after(0.0f, kDarkScene, settings, 30, 2.0f * kFrame);
  CHECK(sixty == doctest::Approx(thirty).epsilon(1e-4));
}

TEST_CASE("a long stall adapts no further than kLongestAdaptStep, and a speed of 0 never adapts") {
  AutoExposure settings;
  CHECK(adapted_ev(0.0f, kBrightScene, settings, kLongWhile) ==
        doctest::Approx(adapted_ev(0.0f, kBrightScene, settings, kLongestAdaptStep)));
  CHECK(adapted_ev(0.0f, kBrightScene, settings, -1.0f) == doctest::Approx(0.0f));  // a clock set back
  settings.speed_up = 0.0f;
  CHECK(adapted_ev(0.0f, kBrightScene, settings, kLongestAdaptStep) == doctest::Approx(0.0f));
}

TEST_CASE("a cut to another camera or a new scene starts the exposure anew") {
  ExposureHistory history;
  CHECK(starts_anew(history, kCamera, kScene));  // nothing adapted yet
  history = {true, kCamera, kScene, 1.0f};
  CHECK_FALSE(starts_anew(history, kCamera, kScene));
  CHECK(starts_anew(history, kOtherCamera, kScene));
  CHECK(starts_anew(history, kCamera, kScene + 1));
  CHECK(adapt_seconds(history, 1.0f + kFrame) == doctest::Approx(kFrame));
  CHECK(adapt_seconds(history, kLongWhile) == doctest::Approx(kLongestAdaptStep));
  CHECK(adapt_seconds(history, 0.0f) == doctest::Approx(0.0f));  // a clock set back
}

TEST_CASE("what a game's camera sets wins over the scene, and what it leaves follows the scene") {
  AutoExposure scene;
  scene.compensation = 0.5f;
  AutoExposureOverrides camera;
  const AutoExposure same = with_overrides(scene, camera);
  CHECK_FALSE(same.enabled);
  CHECK(same.compensation == doctest::Approx(scene.compensation));
  camera.enabled = true;
  camera.min_ev = 2.0f;  // above the scene's brightest: that is raised to it
  const AutoExposure changed = with_overrides(scene, camera);
  CHECK(changed.enabled);
  CHECK(changed.min_ev == doctest::Approx(2.0f));
  CHECK(changed.max_ev == doctest::Approx(2.0f));
  CHECK(changed.speed_up == doctest::Approx(scene.speed_up));
}

TEST_CASE("auto exposure is off unless a scene turns it on") {
  const Environment env = default_environment();
  CHECK_FALSE(env.auto_exposure.enabled);
  CHECK(env.auto_exposure.min_ev == doctest::Approx(kDarkestExposureEv));
  CHECK(env.auto_exposure.max_ev == doctest::Approx(kBrightestExposureEv));
  CHECK(env.auto_exposure.compensation == doctest::Approx(0.0f));
}

TEST_CASE("auto exposure's settings written as a scene's properties read back the same") {
  AutoExposure written;
  written.enabled = true;
  written.min_ev = -6.5f;
  written.max_ev = 2.25f;
  written.speed_up = 4.0f;
  written.speed_down = 0.5f;
  written.compensation = -0.75f;
  PropertyMap props;
  props["environment"] = true;
  write_auto_exposure(written, props);
  const AutoExposure read = environment_from_props(props).auto_exposure;
  CHECK(read.enabled);
  CHECK(read.min_ev == doctest::Approx(written.min_ev));
  CHECK(read.max_ev == doctest::Approx(written.max_ev));
  CHECK(read.speed_up == doctest::Approx(written.speed_up));
  CHECK(read.speed_down == doctest::Approx(written.speed_down));
  CHECK(read.compensation == doctest::Approx(written.compensation));
}

TEST_CASE("a scene's auto exposure settings are held to what makes sense") {
  PropertyMap props;
  props["auto_exposure"] = 1.0;  // a toggle stored as a number, as Blender may
  props["exposure_min_ev"] = -100.0;
  props["exposure_max_ev"] = -200.0;  // below the darkest
  props["exposure_speed_up"] = -1.0;
  props["exposure_compensation"] = std::string("bright");  // not a number: the default stays
  const AutoExposure read = environment_from_props(props).auto_exposure;
  CHECK(read.enabled);
  CHECK(read.min_ev == doctest::Approx(-kWidestExposureEv));
  CHECK(read.max_ev == doctest::Approx(read.min_ev));
  CHECK(read.speed_up == doctest::Approx(0.0f));
  CHECK(read.compensation == doctest::Approx(0.0f));
}

TEST_CASE("leaves let some sunlight through on a clear day, as much as a scene says") {
  CHECK(default_environment().foliage_translucency > 0.0f);
  PropertyMap props;
  props["foliage_translucency"] = 3.0;  // past full: full
  CHECK(environment_from_props(props).foliage_translucency == doctest::Approx(1.0f));
  props["foliage_translucency"] = 0.0;
  CHECK(environment_from_props(props).foliage_translucency == doctest::Approx(0.0f));
}
