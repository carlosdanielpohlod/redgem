# The menu's controls page as a screen, without a game: reached from the settings screen and back,
# what it lays out, and the keys and gamepad buttons it hears (said by the test) picking a player,
# a cell and a capture. (What it holds and does: game_menu_controls_page_test.rb.)
class << UI
  # What the test presses this frame: [keys, gamepad buttons] pairs, as UI's constants name them.
  attr_accessor :pressed_now
  # The box the test clicks this frame ({x:, y:, width:, height:}), if any.
  attr_accessor :clicked_now

  def pressed_any?(controls) = (pressed_now || []).include?(controls)
  def mouse_over?(**box) = clicked_now == box
  def clicked?(over) = over
end

# Counts how often the settings are kept.
class << Settings
  attr_accessor :saves

  alias __save_for_real save

  def save
    self.saves = saves.to_i + 1
    __save_for_real
  end
end

class GameMenuControlsTest < RedGemTest::Case
  KEYS = GameMenu::ControlsPage::KEYS
  BUTTONS = GameMenu::ControlsPage::BUTTONS
  SECOND_PAD = 1
  JUMP = GameMenu::ControlsPage::Row.new(:jump, nil)
  RUN = GameMenu::ControlsPage::Row.new(:run, nil)

  def setup
    Input.cancel_capture
    Input.reset_bindings
    UI.pressed_now = []
    UI.clicked_now = nil
  end

  def menu
    menu = GameMenu.new
    menu.__configure({ title: "Coin Rush", start_open: false })
    menu.start
    menu
  end

  # A menu showing the controls page, and the page.
  def opened_page
    opened = menu
    opened.show(:controls)
    [opened, opened.screens[:controls]]
  end

  def pressing(*controls)
    UI.pressed_now = controls
    yield
  ensure
    UI.pressed_now = []
  end

  def test_the_settings_screen_leads_to_the_controls_page_and_back
    opened = menu
    opened.show_settings
    settings = opened.screens[:settings]
    assert_equal %w[Controls Back], settings.options.map(&:first)
    settings.options.first.last.call
    assert_equal :controls, opened.screen
    opened.screens[:controls].back.call
    assert_equal :settings, opened.screen
  end

  def test_the_settings_go_back_where_they_were_opened_from_after_the_controls_page
    opened = menu
    opened.show(:pause)
    opened.show_settings
    opened.show(:controls)
    opened.screens[:controls].back.call
    opened.screens[:settings].back.call
    assert_equal :pause, opened.screen
  end

  def test_the_page_has_the_players_choice_then_its_rows_then_reset_and_back
    controls = menu.screens[:controls]
    assert_equal 1 + controls.page.rows.size, controls.rows
    assert_equal ["Reset to defaults", "Back"], controls.options.map(&:first)
    assert_equal "Controls", controls.heading
    assert controls.space_above.positive?
  end

  def test_keys_and_a_gamepad_pick_a_cell_and_confirming_it_waits_for_its_binding
    opened, controls = opened_page
    pressing(UI::DOWN) { opened.update(0) }
    pressing(UI::RIGHT) { opened.update(0) }
    assert_equal BUTTONS, controls.column
    pressing(UI::CONFIRM) { opened.update(0) }
    assert_equal [controls.page.rows.first, BUTTONS], controls.page.waiting
  end

  def test_the_page_says_where_it_drew_each_cell
    opened, controls = opened_page
    opened.update(0)
    assert controls.cells.key?([:jump, nil, KEYS])
    assert controls.cells.key?([:horizontal, :negative, BUTTONS])
  end

  def test_leaving_the_page_calls_a_capture_off
    _opened, controls = opened_page
    controls.page.capture(JUMP, KEYS)
    controls.back.call
    refute Input.capturing?
    refute controls.page.capturing?
  end

  def test_the_players_choice_shows_another_players_bindings
    opened, controls = opened_page
    pressing(UI::RIGHT) { opened.update(0) }
    assert_equal SECOND_PAD, controls.page.player
    assert_equal KEYS, controls.column, "on the players' choice, right is the choice's"
  end

  def test_a_cells_text_too_wide_for_it_is_cut_short_and_keeps_its_mark
    controls = menu.screens[:controls]
    size = 15
    fitted = controls.__send__(:fitted, "Left Ctrl, Enter, Left Shift", 120, size, " !")
    assert fitted.end_with?("... !"), fitted
    assert UI.text_width(fitted, size: size) <= 120
    assert_equal "Space !", controls.__send__(:fitted, "Space", 120, size, " !")
  end

  def test_left_and_right_on_its_buttons_leave_the_cell_as_it_was
    opened, controls = opened_page
    controls.focus.move_to(controls.rows) # Reset to defaults
    pressing(UI::RIGHT) { opened.update(0) }
    assert_equal KEYS, controls.column
  end

  def test_a_click_on_a_cell_moves_the_focus_and_the_column_there_and_waits_for_it
    opened, controls = opened_page
    opened.update(0)
    UI.clicked_now = controls.cells.fetch([:run, nil, BUTTONS])
    opened.update(0)
    assert controls.focus.at?(1 + Input.actions.index(:run))
    assert_equal BUTTONS, controls.column
    assert_equal [RUN, BUTTONS], controls.page.waiting
  end

  def test_going_to_the_controls_page_keeps_the_settings_and_coming_back_focuses_its_button
    opened = menu
    opened.show_settings
    settings = opened.screens[:settings]
    Settings.saves = 0
    settings.options.first.last.call
    assert_equal 1, Settings.saves
    opened.screens[:controls].back.call
    assert settings.focus.at?(settings.rows), "on the Controls button"
  end

  def test_a_games_own_options_come_after_the_engines_on_both_screens
    opened = menu
    opened.screens[:controls].add_option("Help") { nil }
    opened.screens[:settings].add_option("Credits", index: 0) { nil }
    assert_equal ["Reset to defaults", "Back", "Help"], opened.screens[:controls].options.map(&:first)
    assert_equal %w[Credits Controls Back], opened.screens[:settings].options.map(&:first)
  end

  def test_the_focus_comes_back_to_the_controls_button_wherever_a_game_put_its_options
    opened = menu
    settings = opened.screens[:settings]
    settings.add_option("Credits", index: 0) { nil }
    opened.show(:settings)
    settings.focus_controls
    assert settings.focus.at?(settings.rows + 1)
  end
end
