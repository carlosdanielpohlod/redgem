# The menu's controls page as a screen, without a game: reached from the settings screen and back,
# what it lays out, and the keys and gamepad buttons it hears (said by the test) picking a player,
# a cell and a capture. (What it holds and does: game_menu_controls_page_test.rb.)
class << UI
  # What the test presses this frame: [keys, gamepad buttons] pairs, as UI's constants name them.
  attr_accessor :pressed_now

  def pressed_any?(controls) = (pressed_now || []).include?(controls)
end

class GameMenuControlsTest < RedGemTest::Case
  KEYS = GameMenu::ControlsPage::KEYS
  BUTTONS = GameMenu::ControlsPage::BUTTONS
  SECOND_PAD = 1
  JUMP = GameMenu::ControlsPage::Row.new(:jump, nil)

  def setup
    Input.cancel_capture
    Input.reset_bindings
    UI.pressed_now = []
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
end
