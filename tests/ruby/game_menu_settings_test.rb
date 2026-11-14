# The menu's settings screen, without a game: its audio section has a slider for the master volume
# and for each group's, and moving one changes that volume at once; its video section ends with the
# interface's size, which the player steps through.
class << UI
  # What the test moves every slider to while drawing (nil: they stay where they are).
  attr_accessor :slid_to
  # The label of the choice the test steps to its next value while drawing (nil: none).
  attr_accessor :stepped

  alias __slider_for_real slider
  alias __choice_for_real choice

  def slider(label, value, **box) = slid_to || __slider_for_real(label, value, **box)

  def choice(label, choices, index, **box)
    picked = __choice_for_real(label, choices, index, **box)
    label == stepped ? (index + 1) % choices.size : picked
  end
end

class GameMenuSettingsTest < RedGemTest::Case
  VOLUMES = %i[volume music_volume effects_volume voice_volume].freeze
  ROWS = 11                    # seven of video, then the four volumes
  FIRST_BUTTON = [0, 576, 280, 58].freeze
  SLID_TO = 0.35

  def settings_screen
    menu = GameMenu.new
    menu.__configure({ title: "Coin Rush", start_open: false })
    menu.start
    menu.screens[:settings]
  end

  def test_the_audio_section_has_the_master_volume_then_each_groups
    assert_equal VOLUMES, GameMenu::SettingsScreen::AUDIO_ROWS
    assert_equal ROWS, settings_screen.rows
  end

  def test_the_video_section_ends_with_the_interfaces_size
    assert_equal :ui_scale, GameMenu::SettingsScreen::VIDEO_ROWS.last
    assert_equal "Interface size", I18n.t("game.settings.ui_scale")
  end

  def test_the_interface_size_steps_to_the_next_one_offered
    Settings.ui_scale = 1.0
    UI.stepped = I18n.t("game.settings.ui_scale")
    settings_screen.draw(FIRST_BUTTON)
    UI.stepped = nil
    assert_in_delta Settings::UI_SCALES[Settings::UI_SCALES.index(1.0) + 1], Settings.ui_scale
  end

  def test_each_volume_is_named_in_words
    names = VOLUMES.map { |volume| GameMenu::SettingsScreen::VOLUME_NAMES.fetch(volume).call }
    assert_equal ["Master volume", "Music volume", "Effects volume", "Voice volume"], names
  end

  def test_a_slider_moved_changes_its_volume_at_once
    VOLUMES.each { |volume| Settings.__send__("#{volume}=", 1.0) }
    UI.slid_to = SLID_TO
    settings_screen.draw(FIRST_BUTTON)
    UI.slid_to = nil
    VOLUMES.each { |volume| assert_in_delta SLID_TO, Settings.__send__(volume) }
  end
end
