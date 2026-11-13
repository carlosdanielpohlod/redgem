// One event of `redgem run --input`: what it reads (keys, mouse, gamepads, touch) and refuses, and
// what it holds on the input the game reads.

#include <doctest/doctest.h>

#include <string>

#include <nlohmann/json.hpp>

#include "project/run_input.hpp"

using namespace redgem;
using namespace redgem::tools;
using Json = nlohmann::json;

namespace {

constexpr long long kFrames = 10;
constexpr RunScreen kScreen{640.0f, 360.0f};

// The event `text` reads as; fails the test when it is refused.
RunInput event_of(const std::string& text) {
  RunInput event;
  const std::string problem = read_run_input(Json::parse(text), kFrames, event);
  REQUIRE_MESSAGE(problem.empty(), problem);
  return event;
}

// Why `text` is refused (empty when it is not).
std::string problem_of(const std::string& text) {
  RunInput event;
  return read_run_input(Json::parse(text), kFrames, event);
}

bool held(const ScriptedInputSource& source, MouseButton button) {
  return source.mouse().buttons[static_cast<std::size_t>(button)];
}

bool pad_held(const ScriptedInputSource& source, std::size_t pad, GamepadButton button) {
  return source.gamepad(pad).buttons[static_cast<std::size_t>(button)];
}

float pad_axis(const ScriptedInputSource& source, std::size_t pad, GamepadAxis axis) {
  return source.gamepad(pad).axes[static_cast<std::size_t>(axis)];
}

}  // namespace

TEST_CASE("a run event reads the keys it holds, as before") {
  const RunInput event = event_of(R"({"frame": 3, "keys": ["w", "space"]})");
  CHECK(event.frame == 3);
  CHECK(event.keys.size() == 2);
  CHECK_FALSE(event.mouse);
  CHECK(event.pads.empty());
  CHECK_FALSE(event.touch);
}

TEST_CASE("a run event reads the mouse: where it goes, buttons held and clicked, the wheel") {
  const RunInput event =
      event_of(R"({"frame": 0, "mouse": {"x": 120, "y": 45.5, "buttons": ["left"], "click": ["right"], "wheel": -2}})");
  REQUIRE(event.mouse);
  REQUIRE(event.mouse->at);
  CHECK(event.mouse->at->x == doctest::Approx(120.0));
  CHECK(event.mouse->at->y == doctest::Approx(45.5));
  CHECK_FALSE(event.mouse->at->normalized);
  CHECK(event.mouse->buttons == std::vector<MouseButton>{MouseButton::Left});
  CHECK(event.mouse->clicks == std::vector<MouseButton>{MouseButton::Right});
  CHECK(event.mouse->wheel == doctest::Approx(-2.0));
  const RunInput stays = event_of(R"({"frame": 0, "mouse": {"buttons": ["middle"]}})");
  CHECK_FALSE(stays.mouse->at);
  CHECK(event_of(R"({"frame": 0, "mouse": {"x": 0.5, "y": 1, "normalized": true}})").mouse->at->normalized);
}

TEST_CASE("a run event reads gamepads by number, with their buttons and axes") {
  const RunInput event = event_of(
      R"({"frame": 1, "pads": [{"buttons": ["a"]}, {"pad": 2, "axes": {"left_x": -1, "right_trigger": 0.5}}]})");
  REQUIRE(event.pads.size() == 2);
  CHECK(event.pads[0].pad == 0);
  CHECK(event.pads[0].buttons == std::vector<GamepadButton>{GamepadButton::A});
  CHECK(event.pads[1].pad == 2);
  REQUIRE(event.pads[1].axes.size() == 2);
  CHECK(event.pads[1].axes[0].axis == GamepadAxis::LeftX);
  CHECK(event.pads[1].axes[0].value == doctest::Approx(-1.0));
}

TEST_CASE("a run event reads a finger held on the screen, or a tap") {
  const RunInput touch = event_of(R"({"frame": 0, "touch": {"x": 10, "y": 20}})");
  REQUIRE(touch.touch);
  CHECK_FALSE(touch.touch->tap);
  CHECK(touch.touch->at.x == doctest::Approx(10.0));
  const RunInput tap = event_of(R"({"frame": 0, "tap": {"x": 0.25, "y": 0.75, "normalized": true}})");
  REQUIRE(tap.touch);
  CHECK(tap.touch->tap);
  CHECK(tap.touch->at.normalized);
}

TEST_CASE("a run event that is not one says why") {
  for (const char* text : {
           R"({"frame": 0})",
           R"({"keys": []})",
           R"([])",
           R"({"frame": 10, "keys": []})",
           R"({"frame": 0.5, "keys": []})",
           R"({"frame": 0, "keys": ["missing"]})",
           R"({"frame": 0, "keys": "w"})",
           R"({"frame": 0, "keys": [], "typo": 1})",
           R"({"frame": 0, "mouse": []})",
           R"({"frame": 0, "mouse": {"x": 1}})",
           R"({"frame": 0, "mouse": {"x": -1, "y": 0}})",
           R"({"frame": 0, "mouse": {"x": 2, "y": 0, "normalized": true}})",
           R"({"frame": 0, "mouse": {"x": 1, "y": 1, "normalized": "yes"}})",
           R"({"frame": 0, "mouse": {"buttons": ["thumb"]}})",
           R"({"frame": 0, "mouse": {"click": [1]}})",
           R"({"frame": 0, "mouse": {"wheel": "up"}})",
           R"({"frame": 0, "mouse": {"wheel": 1000}})",
           R"({"frame": 0, "mouse": {"scroll": 1}})",
           R"({"frame": 0, "pads": {}})",
           R"({"frame": 0, "pads": [{"pad": 4}]})",
           R"({"frame": 0, "pads": [{"pad": 1.5}]})",
           R"({"frame": 0, "pads": [{"pad": 1}, {"pad": 1}]})",
           R"({"frame": 0, "pads": [{"buttons": ["z"]}]})",
           R"({"frame": 0, "pads": [{"axes": {"left_z": 0}}]})",
           R"({"frame": 0, "pads": [{"axes": {"left_x": 2}}]})",
           R"({"frame": 0, "pads": [{"axes": {"left_trigger": -0.5}}]})",
           R"({"frame": 0, "pads": [{"axes": []}]})",
           R"({"frame": 0, "pads": [{"stick": 1}]})",
           R"({"frame": 0, "pads": [{}, {"pad": 1}, {"pad": 2}, {"pad": 3}, {"pad": 3}]})",
           R"({"frame": 0, "touch": {"x": 1}})",
           R"({"frame": 0, "touch": {"x": 1, "y": 1, "id": 0}})",
           R"({"frame": 0, "tap": 3})",
           R"({"frame": 0, "touch": {"x": 1, "y": 1}, "tap": {"x": 1, "y": 1}})",
           R"({"frame": 0, "mouse": {}, "touch": {"x": 1, "y": 1}})",
       }) {
    CAPTURE(text);
    CHECK_FALSE(problem_of(text).empty());
  }
}

TEST_CASE("an event holds what it says on the input the game reads, in place of what was held") {
  ScriptedInputSource source;
  PluggedPads plugged{};
  source.set_key(Key::A, true);
  const RunInput event = event_of(
      R"({"frame": 0, "keys": ["w"], "mouse": {"x": 0.5, "y": 0.25, "normalized": true, "buttons": ["left"]},
          "pads": [{"pad": 1, "buttons": ["b"], "axes": {"left_y": -1}}]})");
  apply_run_input(source, event, kScreen, plugged);
  CHECK_FALSE(source.is_key_down(Key::A));
  CHECK(source.is_key_down(Key::W));
  CHECK(source.mouse().x == doctest::Approx(320.0));
  CHECK(source.mouse().y == doctest::Approx(90.0));
  CHECK(held(source, MouseButton::Left));
  CHECK_FALSE(source.gamepad(0).connected);
  CHECK(source.gamepad(1).connected);
  CHECK(pad_held(source, 1, GamepadButton::B));
  CHECK(pad_axis(source, 1, GamepadAxis::LeftY) == doctest::Approx(-1.0));
  end_run_pulses(source, event);
  CHECK(held(source, MouseButton::Left));
  apply_run_input(source, event_of(R"({"frame": 1, "keys": []})"), kScreen, plugged);
  CHECK_FALSE(held(source, MouseButton::Left));
  CHECK_FALSE(pad_held(source, 1, GamepadButton::B));
  CHECK(pad_axis(source, 1, GamepadAxis::LeftY) == doctest::Approx(0.0));
  CHECK(source.mouse().x == doctest::Approx(320.0));
}

TEST_CASE("a gamepad listed once stays plugged in, with nothing pressed until an event says so") {
  ScriptedInputSource source;
  PluggedPads plugged{};
  apply_run_input(source, event_of(R"({"frame": 0, "pads": [{"pad": 3}]})"), kScreen, plugged);
  CHECK(source.gamepad(3).connected);
  apply_run_input(source, event_of(R"({"frame": 5, "keys": ["w"]})"), kScreen, plugged);
  CHECK(source.gamepad(3).connected);
  CHECK_FALSE(source.gamepad(2).connected);
}

TEST_CASE("a normalized point of 1 is the screen's last pixel") {
  ScriptedInputSource source;
  PluggedPads plugged{};
  apply_run_input(source, event_of(R"({"frame": 0, "tap": {"x": 1, "y": 1, "normalized": true}})"), kScreen, plugged);
  CHECK(source.mouse().x == doctest::Approx(639.0));
  CHECK(source.mouse().y == doctest::Approx(359.0));
}

TEST_CASE("only a click or a tap lasts one frame") {
  CHECK(lasts_one_frame(event_of(R"({"frame": 0, "mouse": {"click": ["left"]}})")));
  CHECK(lasts_one_frame(event_of(R"({"frame": 0, "tap": {"x": 1, "y": 1}})")));
  CHECK_FALSE(lasts_one_frame(event_of(R"({"frame": 0, "mouse": {"buttons": ["left"], "wheel": 1}})")));
  CHECK_FALSE(lasts_one_frame(event_of(R"({"frame": 0, "touch": {"x": 1, "y": 1}})")));
}

TEST_CASE("clicks, the wheel and taps last their frame; a held button and a held finger do not end") {
  ScriptedInputSource source;
  PluggedPads plugged{};
  const RunInput click =
      event_of(R"({"frame": 0, "mouse": {"buttons": ["left"], "click": ["left", "right"], "wheel": 3}})");
  apply_run_input(source, click, kScreen, plugged);
  CHECK(held(source, MouseButton::Right));
  CHECK(source.mouse().wheel == doctest::Approx(3.0));
  end_run_pulses(source, click);
  CHECK(held(source, MouseButton::Left));
  CHECK_FALSE(held(source, MouseButton::Right));
  CHECK(source.mouse().wheel == doctest::Approx(0.0));

  const RunInput tap = event_of(R"({"frame": 1, "tap": {"x": 5, "y": 6}})");
  apply_run_input(source, tap, kScreen, plugged);
  CHECK(source.touch_pointer());
  CHECK(held(source, MouseButton::Left));
  CHECK(source.mouse().x == doctest::Approx(5.0));
  end_run_pulses(source, tap);
  CHECK_FALSE(held(source, MouseButton::Left));

  const RunInput touch = event_of(R"({"frame": 2, "touch": {"x": 7, "y": 8}})");
  apply_run_input(source, touch, kScreen, plugged);
  end_run_pulses(source, touch);
  CHECK(held(source, MouseButton::Left));
  CHECK(source.touch_pointer());
  apply_run_input(source, event_of(R"({"frame": 3, "keys": []})"), kScreen, plugged);
  CHECK_FALSE(source.touch_pointer());
  CHECK_FALSE(held(source, MouseButton::Left));
}

TEST_CASE("a click or a tap of a button the event before still holds would show no press") {
  const RunInput holds_left = event_of(R"({"frame": 0, "mouse": {"buttons": ["left"]}})");
  const RunInput finger = event_of(R"({"frame": 0, "touch": {"x": 1, "y": 1}})");
  const RunInput tap = event_of(R"({"frame": 5, "tap": {"x": 1, "y": 1}})");
  const RunInput left_click = event_of(R"({"frame": 5, "mouse": {"click": ["left"]}})");
  const RunInput right_click = event_of(R"({"frame": 5, "mouse": {"click": ["right"]}})");
  CHECK(hides_press(holds_left, tap));
  CHECK(hides_press(holds_left, left_click));
  CHECK(hides_press(finger, tap));
  CHECK(hides_press(finger, left_click));
  CHECK_FALSE(hides_press(holds_left, right_click));
  CHECK_FALSE(hides_press(tap, left_click));
  CHECK_FALSE(hides_press(event_of(R"({"frame": 0, "keys": []})"), tap));
}
