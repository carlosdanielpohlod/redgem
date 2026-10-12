# The engine's menu, without a game: which screen is open, what freezes, what a game adds to it.
# (Its keys and buttons are tested with games running: tests/system/platformer, tests/system_templates.)
class GameMenuTest < RedGemTest::Case
  # A screen that keeps count of being shown and left.
  class Watched < GameMenu::Screen
    attr_reader :shown, :left

    def opened = @shown = (@shown || 0) + 1
    def closed = @left = (@left || 0) + 1
  end

  def menu(**settings)
    menu = GameMenu.new
    menu.__configure({ title: "Coin Rush", start_open: false }.merge(settings))
    menu.start
    menu
  end

  def test_it_opens_on_the_title_screen_with_the_game_frozen
    opened = menu(start_open: true)
    assert_equal :title, opened.screen
    assert_equal "Coin Rush", opened.screens[:title].heading
    assert_in_delta GameMenu::FROZEN, Time.scale
  end

  def test_start_open_false_starts_the_game_right_away
    closed = menu
    refute closed.open?
    assert_nil closed.screen
  end

  def test_show_and_resume_freeze_the_game_and_let_it_go_on
    paused = menu
    paused.show(:pause)
    assert paused.open?
    assert_in_delta GameMenu::FROZEN, Time.scale
    paused.resume
    refute paused.open?
    assert_in_delta GameMenu::RUNNING, Time.scale
  end

  def test_finish_shows_the_end_screen_under_the_message
    over = menu
    over.finish("You win!")
    assert_equal :over, over.screen
    assert_equal "You win!", over.screens[:over].heading
  end

  def test_the_screens_offer_what_a_game_expects
    labels = ->(screen) { menu.screens[screen].options.map(&:first) }
    assert_equal %w[Play Quit], labels.call(:title)
    assert_equal %w[Resume Restart Quit], labels.call(:pause)
    assert_equal ["Play again", "Quit"], labels.call(:over)
  end

  def test_an_option_is_added_where_asked
    pause = menu.screens[:pause]
    pause.add_option("Credits", index: 1) { :credits }
    pause.add_option("Last") { :last }
    assert_equal %w[Resume Credits Restart Quit Last], pause.options.map(&:first)
    assert_equal :credits, pause.options[1].last.call
  end

  def test_resume_closes_the_menu_from_its_button_and_from_the_pause_keys
    paused = menu
    pause = paused.screens[:pause]
    [pause.options.first.last, pause.back].each do |way_out|
      paused.show(:pause)
      way_out.call
      refute paused.open?
    end
    assert_nil paused.screens[:title].back
  end

  def test_a_screen_of_the_games_own_hears_when_it_is_shown_and_left
    extended = menu
    watched = extended.screens[:credits] = Watched.new(heading: "Credits")
    extended.show(:credits)
    assert_equal [1, nil], [watched.shown, watched.left]
    extended.show(:pause)
    extended.show(:credits)
    extended.resume
    assert_equal [2, 2], [watched.shown, watched.left]
  end

  def test_a_screen_says_what_the_pause_keys_do_on_it
    extended = menu
    extended.screens[:credits] = GameMenu::Screen.new(back: -> { extended.show(:pause) })
    extended.show(:credits)
    extended.screens[:credits].back.call
    assert_equal :pause, extended.screen
  end

  def test_a_plain_screen_shows_nothing_more_than_its_buttons
    screen = GameMenu::Screen.new
    assert_equal "", screen.heading
    assert_equal 0, screen.space_above
    refute screen.typing?
    assert_nil screen.draw([0, 0, 0, 0])
  end

  def test_buttons_are_stacked_in_the_middle_of_the_screen
    stacked = menu
    x, y, width, height = stacked.button_box(0)
    assert_equal [(UI.width - width) / 2, GameMenu::BUTTON_WIDTH, GameMenu::BUTTON_HEIGHT], [x, width, height]
    assert_equal y + height + GameMenu::BUTTON_GAP, stacked.button_box(1)[1]
  end

  def test_a_scene_with_no_menu_says_so
    error = assert_raises(ArgumentError) { GameMenu.instance }
    assert_includes_text error.message, "no object of the scene has a GameMenu"
  end

  def assert_includes_text(text, part) = assert(text.include?(part), "#{text.inspect} has no #{part.inspect}")
end
