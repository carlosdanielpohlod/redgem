# Settings screens' widgets and the focus that keys and a gamepad move between them, with the keys
# and buttons pressed said by the test (the mouse is nowhere near the widgets).
class << UI
  # What the test presses this frame: [keys, gamepad buttons] pairs, as UI's constants name them.
  attr_accessor :pressed_now

  def pressed_any?(controls) = (pressed_now || []).include?(controls)
end

class UiWidgetsTest < RedGemTest::Case
  BOX = { x: 100, y: 100, width: 400, height: 40 }.freeze

  def setup
    UI.pressed_now = []
  end

  def pressing(*controls)
    UI.pressed_now = controls
    yield
  ensure
    UI.pressed_now = []
  end

  def test_the_focus_moves_up_and_down_and_wraps_around
    focus = UI::Focus.new
    assert focus.at?(0)
    pressing(UI::DOWN) { focus.update(3) }
    assert_equal 1, focus.index
    pressing(UI::UP) { 2.times { focus.update(3) } }
    assert_equal 2, focus.index
  end

  def test_the_focus_stays_while_nothing_is_pressed_and_goes_back_to_the_first_when_reset
    focus = UI::Focus.new
    pressing(UI::UP) { focus.update(3) }
    focus.update(3)
    assert_equal 2, focus.index
    focus.reset
    assert focus.at?(0)
  end

  def test_a_focus_over_nothing_stays_on_the_first
    focus = UI::Focus.new
    pressing(UI::DOWN) { focus.update(0) }
    assert_equal 0, focus.index
  end

  def test_a_focused_button_is_clicked_by_the_confirm_key
    assert(pressing(UI::CONFIRM) { UI.button("Go", **BOX, focused: true) })
    refute(pressing(UI::CONFIRM) { UI.button("Go", **BOX) }, "only the focused one")
    refute UI.button("Go", **BOX, focused: true), "nothing pressed"
  end

  def test_a_toggle_flips_when_confirmed_or_pushed_sideways_while_focused
    assert_equal true, pressing(UI::CONFIRM) { UI.toggle("VSync", false, **BOX, focused: true) }
    assert_equal false, pressing(UI::LEFT) { UI.toggle("VSync", true, **BOX, focused: true) }
    assert_equal true, pressing(UI::RIGHT) { UI.toggle("VSync", true, **BOX) }
    assert_equal true, UI.toggle("VSync", true, **BOX, focused: true)
  end

  def test_a_choice_goes_left_and_right_and_wraps_and_a_confirm_goes_to_the_next
    choices = %w[Low Medium High]
    assert_equal 2, pressing(UI::RIGHT) { UI.choice("Quality", choices, 1, **BOX, focused: true) }
    assert_equal 2, pressing(UI::LEFT) { UI.choice("Quality", choices, 0, **BOX, focused: true) }
    assert_equal 0, pressing(UI::CONFIRM) { UI.choice("Quality", choices, 2, **BOX, focused: true) }
    assert_equal 1, pressing(UI::RIGHT) { UI.choice("Quality", choices, 1, **BOX) }
  end

  def test_a_slider_moves_a_step_sideways_and_stays_within_its_range
    assert_in_delta 0.55, pressing(UI::RIGHT) { UI.slider("Volume", 0.5, **BOX, focused: true) }
    assert_in_delta 0.4, pressing(UI::LEFT) { UI.slider("Volume", 0.5, **BOX, step: 0.1, focused: true) }
    assert_in_delta 1.0, pressing(UI::RIGHT) { UI.slider("Volume", 1.0, **BOX, focused: true) }
    assert_in_delta 3.0, UI.slider("Speed", 7.0, **BOX, min: 1.0, max: 3.0)
  end

  def test_a_sliders_track_is_on_the_right_of_its_row
    track = UI.slider_track(BOX)
    assert_equal BOX[:x] + BOX[:width] - UI::FIELD_PADDING, track[:x] + track[:width]
    assert_equal [BOX[:y], BOX[:height]], [track[:y], track[:height]]
  end

  def test_left_and_right_together_go_nowhere
    assert_equal 0, pressing(UI::LEFT, UI::RIGHT) { UI.sideways }
    assert_equal(-1, pressing(UI::LEFT) { UI.sideways })
  end
end
