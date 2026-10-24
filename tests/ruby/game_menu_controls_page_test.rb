# The menu's controls page as it holds and does things, without a game and without drawing: its
# rows and what their cells show, a capture filling a cell (or called off), each player's own
# bindings, going back to the game's, and keys bound to two rows. (Drawn and worked by keys, a
# gamepad and the mouse: game_menu_controls_test.rb, and with a game running, kept for the next
# run: tests/system/platformer/display_settings_test.rb.)
class GameMenuControlsPageTest < RedGemTest::Case
  KEYS = GameMenu::ControlsPage::KEYS
  BUTTONS = GameMenu::ControlsPage::BUTTONS
  SECOND_PAD = 1
  Row = GameMenu::ControlsPage::Row
  JUMP = Row.new(:jump, nil)
  RUN = Row.new(:run, nil)
  LEFT = Row.new(:horizontal, :negative)
  RIGHT = Row.new(:horizontal, :positive)

  def setup
    Input.cancel_capture
    Input.reset_bindings
  end

  def page = GameMenu::ControlsPage.new

  def test_a_row_for_every_action_then_for_both_sides_of_every_axis
    rows = page.rows
    actions = Input.actions
    assert_equal actions, rows.first(actions.size).map(&:name)
    horizontal = rows.select { |row| row.name == :horizontal }
    assert_equal [LEFT, RIGHT], horizontal
    assert_equal actions.size + (2 * Input.axes.size), rows.size
  end

  def test_rows_are_named_in_words
    assert_equal "Jump", GameMenu::BindingNames.row(:jump, nil)
    assert_equal "Move left", GameMenu::BindingNames.row(:horizontal, :negative)
    Input.define_action(:air_dash, :e)
    Input.define_axis(:steer, negative: [:j], positive: [:l])
    assert_equal "Air dash", GameMenu::BindingNames.row(:air_dash, nil)
    assert_equal "Steer +", GameMenu::BindingNames.row(:steer, :positive)
    assert_equal "None", page.text(Row.new(:steer, :negative), BUTTONS), "no stick moves it"
  end

  def test_bindings_are_named_as_players_know_them
    bindings = %w[key:space key:k key:7 key:left_shift key:comma pad:a pad:dpad_up]
    names = bindings.map { |binding| GameMenu::BindingNames.of(binding) }
    assert_equal ["Space", "K", "7", "Left Shift", ",", "A button", "D-pad up"], names
    assert_equal "Left stick X", GameMenu::BindingNames.stick(:left_x)
  end

  def test_a_cell_shows_its_keys_or_its_buttons_and_an_axis_its_stick_among_the_buttons
    shown = page
    assert_equal ["Space", "A button"], [shown.text(JUMP, KEYS), shown.text(JUMP, BUTTONS)]
    assert_equal "Left Ctrl, Enter", shown.text(Row.new(:fire, nil), KEYS)
    assert_equal "Left stick X", shown.text(LEFT, BUTTONS)
    Input.unbind(:jump, key: :space)
    assert_equal "None", shown.text(JUMP, KEYS)
  end

  def test_the_key_a_capture_catches_takes_the_place_of_the_cells_keys_and_the_buttons_stay
    rebinding = page
    rebinding.capture(JUMP, KEYS)
    assert Input.capturing?
    assert_equal [JUMP, KEYS], rebinding.waiting
    Input.__captured("key:k", nil)
    assert_equal ["key:k", "pad:a"], Input.bindings(:jump)
    refute rebinding.capturing?
  end

  def test_the_button_a_capture_catches_takes_the_place_of_a_sides_buttons_and_its_keys_stay
    page.capture(LEFT, BUTTONS)
    Input.__captured("pad:x", Input::FIRST_PAD)
    assert_equal ["key:a", "key:left", "pad:x"], Input.bindings(:horizontal)[:negative]
    assert_equal ["key:d", "key:right"], Input.bindings(:horizontal)[:positive]
  end

  def test_a_binding_of_the_other_kind_is_not_for_the_cell_and_it_waits_on
    rebinding = page
    rebinding.capture(JUMP, KEYS)
    Input.__captured("pad:y", Input::FIRST_PAD)
    assert rebinding.capturing?
    assert_equal ["key:space", "pad:a"], Input.bindings(:jump)
    Input.__captured("key:j", nil)
    assert_equal ["key:j", "pad:a"], Input.bindings(:jump)
  end

  def test_b_calls_a_wait_off_for_a_keys_cell_but_can_be_bound_in_a_buttons_cell
    cancels = GameMenu::ControlsPage::CANCELS
    assert_equal Input::CANCEL_CAPTURE, cancels.fetch(KEYS)
    refute cancels.fetch(BUTTONS).include?("pad:b")
    assert cancels.fetch(BUTTONS).include?("key:escape")
  end

  def test_a_game_names_its_own_rows_apart_from_the_pages_other_texts
    Input.define_action(:reset, :r)
    assert_equal "Reset", GameMenu::BindingNames.row(:reset, nil)
  end

  def test_a_capture_called_off_leaves_the_cell_as_it_was
    rebinding = page
    rebinding.capture(JUMP, BUTTONS)
    Input.__captured(nil, nil)
    refute rebinding.capturing?
    assert_equal ["key:space", "pad:a"], Input.bindings(:jump)
    rebinding.capture(JUMP, BUTTONS)
    rebinding.cancel
    refute Input.capturing?
  end

  def test_showing_another_players_bindings_calls_a_capture_off
    shown = page
    shown.capture(JUMP, KEYS)
    shown.player = SECOND_PAD
    refute shown.capturing?
    assert_equal SECOND_PAD, shown.player
  end

  def test_each_player_rebinds_their_own_and_resets_their_own
    second = page
    second.player = SECOND_PAD
    second.capture(JUMP, BUTTONS)
    Input.__captured("pad:y", SECOND_PAD)
    assert_equal ["pad:y"], Input.bindings(:jump, pad: SECOND_PAD), "(the keyboard is the first player's)"
    assert_equal ["key:space", "pad:a"], Input.bindings(:jump)
  end

  def test_reset_puts_back_the_shown_players_bindings_only
    second = page
    second.player = SECOND_PAD
    Input.bind(:jump, button: :y, pad: SECOND_PAD)
    Input.bind(:run, key: :k)
    second.reset
    assert_equal ["pad:a"], Input.bindings(:jump, pad: SECOND_PAD)
    assert_equal ["key:left_shift", "key:k", "pad:b"], Input.bindings(:run), "the first player's stay"
  end

  def test_a_key_bound_to_two_rows_is_allowed_and_marked_on_both
    marked = page
    assert_equal [], marked.shared
    Input.bind(:run, key: :space)
    assert_equal ["key:space"], marked.shared
    assert marked.conflict?(JUMP, KEYS)
    assert marked.conflict?(RUN, KEYS)
    refute marked.conflict?(JUMP, BUTTONS)
    refute marked.conflict?(LEFT, KEYS)
  end
end
