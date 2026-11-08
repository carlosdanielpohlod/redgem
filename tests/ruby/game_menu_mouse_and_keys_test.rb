# The menu's controls page with the whole keyboard and the mouse: what it calls every kind of key
# and mouse binding, and a keys' cell that a mouse button or the wheel fills too (a buttons' cell
# never does). (That the click asking for a capture, and the release of the one caught, click
# nothing: tests/integration/input_actions_test.cpp and tests/unit/mouse_bindings_test.cpp.)
class GameMenuMouseAndKeysTest < RedGemTest::Case
  KEYS = GameMenu::ControlsPage::KEYS
  BUTTONS = GameMenu::ControlsPage::BUTTONS
  JUMP = GameMenu::ControlsPage::Row.new(:jump, nil)

  def setup
    Input.cancel_capture
    Input.reset_bindings
  end

  def page = GameMenu::ControlsPage.new
  def names(bindings) = bindings.map { |binding| GameMenu::BindingNames.of(binding) }

  def test_the_function_editing_and_lock_keys_are_named_in_words_or_capitals
    bindings = %w[key:f5 key:f12 key:delete key:page_up key:home key:caps_lock key:right_alt key:print_screen]
    expected = ["F5", "F12", "Delete", "Page Up", "Home", "Caps Lock", "Right Alt", "Print Screen"]
    assert_equal expected, names(bindings)
  end

  def test_the_punctuation_keys_show_the_character_they_type
    bindings = %w[key:semicolon key:equals key:left_bracket key:right_bracket key:backslash key:grave]
    assert_equal [";", "=", "[", "]", "\\", "`"], names(bindings)
  end

  def test_the_keypad_keys_are_named_after_it
    bindings = %w[key:kp_0 key:kp_7 key:kp_add key:kp_decimal key:kp_enter]
    assert_equal ["Numpad 0", "Numpad 7", "Numpad +", "Numpad .", "Numpad Enter"], names(bindings)
  end

  def test_the_mouse_buttons_and_wheel_are_named_as_players_know_them
    bindings = %w[mouse:left mouse:right mouse:middle mouse:side mouse:extra mouse:wheel_up mouse:wheel_down]
    expected = ["Left mouse button", "Right mouse button", "Middle mouse button", "Mouse button 4",
                "Mouse button 5", "Mouse wheel up", "Mouse wheel down"]
    assert_equal expected, names(bindings)
  end

  def test_a_keys_cell_shows_the_rows_mouse_bindings_too
    Input.bind(:jump, mouse: :right)
    shown = page
    assert_equal ["key:space", "mouse:right"], shown.bindings(JUMP, KEYS)
    assert_equal "Space, Right mouse button", shown.text(JUMP, KEYS)
    assert_equal ["pad:a"], shown.bindings(JUMP, BUTTONS)
  end

  def test_a_mouse_button_caught_takes_the_place_of_a_keys_cells_bindings
    page.capture(JUMP, KEYS)
    Input.__captured("mouse:left", nil)
    assert_equal ["mouse:left", "pad:a"], Input.bindings(:jump)
  end

  def test_the_wheel_caught_fills_a_keys_cell
    page.capture(JUMP, KEYS)
    Input.__captured("mouse:wheel_down", nil)
    assert_equal ["mouse:wheel_down", "pad:a"], Input.bindings(:jump)
  end

  def test_a_buttons_cell_is_no_place_for_the_mouse_and_waits_on
    rebinding = page
    rebinding.capture(JUMP, BUTTONS)
    Input.__captured("mouse:left", nil)
    assert rebinding.capturing?
    assert_equal ["key:space", "pad:a"], Input.bindings(:jump)
  end
end
