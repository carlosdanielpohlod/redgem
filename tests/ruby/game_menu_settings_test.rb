# The menu's settings screen, without a game: its audio section has a slider for the master volume
# and for each group's, and moving one changes that volume at once.
class << UI
  # What the test moves every slider to while drawing (nil: they stay where they are).
  attr_accessor :slid_to

  alias __slider_for_real slider

  def slider(label, value, **box) = slid_to || __slider_for_real(label, value, **box)
end

class GameMenuSettingsTest < RedGemTest::Case
  VOLUMES = %i[volume music_volume effects_volume voice_volume].freeze
  ROWS = 10                    # six of video, then the four volumes
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
