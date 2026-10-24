#include <doctest/doctest.h>

#include "editor/gated_input_source.hpp"

using namespace redgem;

namespace {

constexpr float kViewX = 120.0f;  // where the Scene view's image starts in the window
constexpr float kViewY = 48.0f;

}  // namespace

TEST_CASE("a game without the Scene view's focus gets no keys, buttons, wheel, gamepad or text") {
  GatedInputSource input;
  input.enabled = false;
  CHECK_FALSE(input.is_key_down(Key::Space));
  const MouseSample mouse = input.mouse();
  for (const bool held : mouse.buttons) CHECK_FALSE(held);
  CHECK(mouse.wheel == 0.0f);
  CHECK_FALSE(input.gamepad(kFirstGamepad).connected);
  CHECK(input.typed_text().empty());
}

TEST_CASE("the game sees the mouse in the Scene view's own pixels") {
  GatedInputSource in_window;
  GatedInputSource in_view;
  in_view.origin_x = kViewX;
  in_view.origin_y = kViewY;
  const MouseSample whole = in_window.mouse();
  const MouseSample shifted = in_view.mouse();
  CHECK(whole.x - shifted.x == doctest::Approx(kViewX));
  CHECK(whole.y - shifted.y == doctest::Approx(kViewY));
}
