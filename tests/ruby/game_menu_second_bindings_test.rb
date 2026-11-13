# A second key or button for one action (W and Up), on the menu's controls page: each in a cell of
# its own, a key caught for the second cell added beside the first, the first replaced with the
# second kept in its place, clearing one cell, and a key the row already has. First as the page
# holds it, then worked by keys and a gamepad (said by the test). (The rest of the page:
# game_menu_controls_page_test.rb and game_menu_controls_test.rb.)
class << UI
  # What the test presses this frame: [keys, gamepad buttons] pairs, as UI's constants name them.
  attr_accessor :pressed_now

  def pressed_any?(controls) = (pressed_now || []).include?(controls)
  def mouse_over?(**_box) = false
end

class GameMenuSecondBindingsTest < RedGemTest::Case
  KEYS = GameMenu::ControlsPage::KEYS
  BUTTONS = GameMenu::ControlsPage::BUTTONS
  SECOND_PAD = 1
  FIRST = 0
  SECOND = 1
  CLEAR = GameMenu::ControlsScreen::CLEAR
  Row = GameMenu::ControlsPage::Row
  JUMP = Row.new(:jump, nil)
  RUN = Row.new(:run, nil)
  LEFT = Row.new(:horizontal, :negative)
  UP = Row.new(:vertical, :positive)

  def setup
    Input.cancel_capture
    Input.reset_bindings
    UI.pressed_now = []
  end

  def page = GameMenu::ControlsPage.new

  # A menu showing the controls page, and the page.
  def opened_page
    menu = GameMenu.new
    menu.__configure({ title: "Coin Rush", start_open: false })
    menu.start
    menu.show(:controls)
    [menu, menu.screens[:controls]]
  end

  # Puts the focus on the jump action's row (the actions are listed by name).
  def on_jump(controls) = controls.focus.move_to(1 + Input.actions.index(:jump))

  def pressing(*controls)
    UI.pressed_now = controls
    yield
  ensure
    UI.pressed_now = []
  end

  def test_a_row_has_two_cells_for_keys_and_two_for_buttons
    cells = GameMenu::ControlsPage::CELLS
    assert_equal [[KEYS, FIRST], [KEYS, SECOND], [BUTTONS, FIRST], [BUTTONS, SECOND]], cells
  end

  def test_each_of_a_rows_keys_shows_in_a_cell_of_its_own
    shown = page
    assert_equal %w[W Up], [shown.text(UP, KEYS, FIRST), shown.text(UP, KEYS, SECOND)]
    assert_equal "W, Up", shown.text(UP, KEYS)
    assert_equal "None", shown.text(JUMP, KEYS, SECOND)
    Input.bind(:vertical, key: :i, side: :positive)
    assert_equal "Up, I", shown.text(UP, KEYS, SECOND), "the last cell shows any more"
  end

  def test_an_axis_stick_shows_in_its_first_buttons_cell_only
    shown = page
    assert_equal "Left stick X", shown.text(LEFT, BUTTONS, FIRST)
    assert_equal "None", shown.text(LEFT, BUTTONS, SECOND)
  end

  def test_a_key_caught_for_the_second_cell_is_added_and_the_first_stays
    rebinding = page
    rebinding.capture(JUMP, KEYS, SECOND)
    assert_equal [JUMP, KEYS, SECOND], rebinding.waiting
    assert_equal KEYS, rebinding.waiting_column
    Input.__captured("key:k", nil)
    assert_equal ["key:space", "key:k", "pad:a"], Input.bindings(:jump)
    assert_equal %w[Space K], [rebinding.text(JUMP, KEYS, FIRST), rebinding.text(JUMP, KEYS, SECOND)]
  end

  def test_a_key_caught_for_the_first_cell_keeps_the_second_in_its_place
    page.capture(UP, KEYS, FIRST)
    Input.__captured("key:i", nil)
    assert_equal ["key:i", "key:up"], Input.bindings(:vertical)[:positive]
    page.capture(UP, KEYS, SECOND)
    Input.__captured("key:k", nil)
    assert_equal ["key:i", "key:k"], Input.bindings(:vertical)[:positive]
  end

  def test_a_second_button_is_added_beside_the_first
    page.capture(JUMP, BUTTONS, SECOND)
    Input.__captured("pad:y", Input::FIRST_PAD)
    assert_equal ["key:space", "pad:a", "pad:y"], Input.bindings(:jump)
  end

  def test_a_key_the_row_already_has_changes_nothing
    rebinding = page
    rebinding.capture(UP, KEYS, SECOND)
    Input.__captured("key:w", nil)
    refute rebinding.capturing?
    assert_equal ["key:w", "key:up"], Input.bindings(:vertical)[:positive]
  end

  def test_clearing_a_cell_takes_its_binding_away_and_leaves_the_others
    clearing = page
    clearing.clear(UP, KEYS, SECOND)
    assert_equal ["key:w"], Input.bindings(:vertical)[:positive]
    assert_equal ["key:s", "key:down"], Input.bindings(:vertical)[:negative]
    clearing.clear(UP, KEYS, FIRST)
    assert_equal [], Input.bindings(:vertical)[:positive]
    clearing.clear(JUMP, BUTTONS, FIRST)
    assert_equal ["key:space"], Input.bindings(:jump)
  end

  def test_clearing_calls_a_capture_off_and_an_empty_cell_stays_empty
    clearing = page
    clearing.capture(JUMP, KEYS, FIRST)
    clearing.clear(JUMP, KEYS, SECOND)
    refute Input.capturing?
    assert_equal ["key:space", "pad:a"], Input.bindings(:jump)
  end

  def test_a_second_key_on_another_row_marks_that_cell_alone
    marked = page
    Input.bind(:run, key: :space)
    assert marked.conflict?(RUN, KEYS, SECOND)
    refute marked.conflict?(RUN, KEYS, FIRST)
    assert marked.conflict?(RUN, KEYS)
  end

  def test_a_second_players_second_button_is_their_own
    second = page
    second.player = SECOND_PAD
    second.capture(JUMP, BUTTONS, SECOND)
    Input.__captured("pad:y", SECOND_PAD)
    assert_equal ["pad:a", "pad:y"], Input.bindings(:jump, pad: SECOND_PAD)
    assert_equal ["key:space", "pad:a"], Input.bindings(:jump)
  end

  def test_right_stops_at_the_last_cell_and_left_at_the_first
    opened, controls = opened_page
    pressing(UI::DOWN) { opened.update(0) }
    4.times { pressing(UI::RIGHT) { opened.update(0) } }
    assert_equal [BUTTONS, SECOND], [controls.column, controls.slot]
    5.times { pressing(UI::LEFT) { opened.update(0) } }
    assert_equal [KEYS, FIRST], [controls.column, controls.slot]
  end

  def test_confirming_the_second_keys_cell_then_a_key_adds_a_second_key
    opened, controls = opened_page
    on_jump(controls)
    pressing(UI::RIGHT) { opened.update(0) }
    pressing(UI::CONFIRM) { opened.update(0) }
    assert_equal [JUMP, KEYS, SECOND], controls.page.waiting
    Input.__captured("key:k", nil)
    assert_equal ["key:space", "key:k", "pad:a"], Input.bindings(:jump)
  end

  def test_delete_or_y_clears_the_focused_cell_only
    Input.bind(:jump, key: :k)
    opened, controls = opened_page
    on_jump(controls)
    [UI::RIGHT, CLEAR].each { |control| pressing(control) { opened.update(0) } }
    assert_equal ["key:space", "pad:a"], Input.bindings(:jump)
    [UI::LEFT, CLEAR].each { |control| pressing(control) { opened.update(0) } }
    assert_equal ["pad:a"], Input.bindings(:jump)
  end

  def test_nothing_is_cleared_while_a_capture_waits
    opened, controls = opened_page
    on_jump(controls)
    controls.page.capture(JUMP, KEYS, FIRST)
    pressing(CLEAR) { opened.update(0) }
    assert_equal ["key:space", "pad:a"], Input.bindings(:jump)
    assert controls.page.capturing?
  end

  def test_nothing_is_cleared_off_the_rows
    opened, controls = opened_page
    controls.focus.move_to(controls.rows) # Reset to defaults
    pressing(CLEAR) { opened.update(0) }
    assert_equal ["key:space", "pad:a"], Input.bindings(:jump)
  end

  def test_the_note_says_how_to_clear_a_cell_unless_a_mark_shows
    _opened, controls = opened_page
    assert_equal I18n.t("game.controls.clear_note"), controls.__send__(:note_text, [])
    assert_equal I18n.t("game.controls.conflict_note"), controls.__send__(:note_text, ["key:space"])
  end
end
