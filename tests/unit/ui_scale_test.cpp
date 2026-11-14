// How big a unit of a game's interface is on a screen: the reference screen's scale to it, by the
// side it matches, times the player's own scale; and how the player's scale is kept.
#include <doctest/doctest.h>

#include <limits>

#include "render/display_settings.hpp"
#include "render/ui_scale.hpp"

using namespace redgem;

namespace {

constexpr SurfaceSize kReference{1920, 1080};
constexpr SurfaceSize kHd{1280, 720};
constexpr SurfaceSize kUhd{3840, 2160};
constexpr SurfaceSize kSquarish{800, 600};
constexpr float kLarger = 1.5f;

UiReference matching(UiMatch match) { return {kReference, match}; }

}  // namespace

TEST_CASE("a unit is a pixel on the reference screen, and grows and shrinks with the side matched") {
  const UiReference height = matching(UiMatch::Height);
  CHECK(ui_scale(kReference, height, kGamesUiScale) == doctest::Approx(1.0f));
  CHECK(ui_scale(kHd, height, kGamesUiScale) == doctest::Approx(720.0f / 1080.0f));
  CHECK(ui_scale(kUhd, height, kGamesUiScale) == doctest::Approx(2.0f));
  CHECK(ui_scale(kSquarish, height, kGamesUiScale) == doctest::Approx(600.0f / 1080.0f));
  CHECK(ui_scale(kSquarish, matching(UiMatch::Width), kGamesUiScale) == doctest::Approx(800.0f / 1920.0f));
  // Fit keeps all of the reference on the screen: the smaller of the two.
  CHECK(ui_scale(kSquarish, matching(UiMatch::Fit), kGamesUiScale) == doctest::Approx(800.0f / 1920.0f));
  CHECK(ui_scale({3840, 1080}, matching(UiMatch::Fit), kGamesUiScale) == doctest::Approx(1.0f));
}

TEST_CASE("the player's scale multiplies the reference's, held from half to twice") {
  const UiReference height = matching(UiMatch::Height);
  CHECK(ui_scale(kUhd, height, kLarger) == doctest::Approx(2.0f * kLarger));
  CHECK(ui_scale(kReference, height, 9.0f) == doctest::Approx(kLargestUiScale));
  CHECK(ui_scale(kReference, height, 0.0f) == doctest::Approx(kSmallestUiScale));
  CHECK(held_ui_scale(std::numeric_limits<float>::quiet_NaN()) == kGamesUiScale);
}

TEST_CASE("with no screen or no reference to go by, a unit is a pixel (times the player's scale)") {
  CHECK(ui_scale({0, 0}, matching(UiMatch::Height), kGamesUiScale) == doctest::Approx(1.0f));
  CHECK(ui_scale(kUhd, UiReference{}, kGamesUiScale) == doctest::Approx(1.0f));
  CHECK(ui_scale(kUhd, UiReference{}, kLarger) == doctest::Approx(kLarger));
}

TEST_CASE("matches go by their names, and nothing else is one") {
  for (const UiMatch match : {UiMatch::Width, UiMatch::Height, UiMatch::Fit}) {
    CHECK(ui_match_from_name(ui_match_name(match)) == match);
  }
  CHECK_FALSE(ui_match_from_name("Height"));
  CHECK_FALSE(ui_match_from_name("stretch"));
}

TEST_CASE("the player's interface scale is kept with the other settings, and held to its range") {
  DisplaySettings display;
  CHECK(display.ui_scale == kGamesUiScale);
  display.ui_scale = kLarger;
  const DisplaySettings read = display_from_json(display_json(display), DisplaySettings{});
  CHECK(read.ui_scale == doctest::Approx(kLarger));
  CHECK(display_from_json({{"ui_scale", 5.0}}, read).ui_scale == kLargestUiScale);
  CHECK(display_from_json({{"ui_scale", "big"}}, read).ui_scale == doctest::Approx(kLarger));
}

TEST_CASE("a player's interface scale far past a float's range is held to the largest") {
  CHECK(display_from_json({{"ui_scale", 1e300}}, DisplaySettings{}).ui_scale == kLargestUiScale);
  CHECK(display_from_json({{"ui_scale", -1e300}}, DisplaySettings{}).ui_scale == kSmallestUiScale);
}
