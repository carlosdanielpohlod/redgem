# The menu's journal (game_menu_journal.rb, game_menu_journal_screen.rb), without a game: what it
# lists of the quests under way and completed, which quest is tracked, and where the pause menu
# offers it. The quests are given as the engine reads their files (string keys).
class << UI
  # Whether the test picks the focused widget while drawing (as Enter or A would).
  attr_accessor :pick_focused

  alias __worked_for_real worked?

  def worked?(box, focused, *confirm) = (pick_focused && focused) || __worked_for_real(box, focused, *confirm)
end

class QuestJournalTest < RedGemTest::Case
  ERRAND = {
    "title" => "errand.title", "start" => "go",
    "stages" => {
      "go" => { "text" => "errand.go", "objectives" => [
        { "go_to" => "Well", "text" => "errand.well" },
        { "collect" => "water", "count" => 2, "text" => "errand.water" }
      ], "next" => "done" },
      "done" => { "text" => "errand.done", "end" => true }
    }
  }.freeze
  # A quest whose stage says nothing of itself.
  UNTOLD = { "title" => "mute.title", "start" => "go", "stages" => {
    "go" => { "objectives" => [{ "collect" => "water", "text" => "mute.water" }], "next" => "done" },
    "done" => { "text" => "mute.done", "end" => true }
  } }.freeze
  NAMES = %w[well mill barn].freeze
  FIRST_BUTTON = [0, 619, 280, 58].freeze

  def setup
    Quests.__scene_reset
    GameVariables.replace({})
    Quests.track(nil)
    NAMES.each { |name| Quests.instance_variable_get(:@definitions)[name] = Quests::Definition.new(name, ERRAND) }
  end

  def menu(**settings)
    menu = GameMenu.new
    menu.__configure({ title: "Errands", start_open: false }.merge(settings))
    menu.start
    menu
  end

  # The journal page of a new menu, its focus on a line.
  def focused_journal(index)
    journal = menu(journal: true).screens[:journal]
    journal.focus = UI::Focus.new
    journal.focus.move_to(index)
    journal
  end

  def lines = GameMenu::Journal.lines
  def kinds = lines.map(&:kind)

  def test_with_nothing_under_way_it_says_so
    assert_equal %i[section none], kinds
    assert_equal ["Under way", "No quest is under way"], lines.map(&:text)
  end

  def test_a_quest_under_way_shows_its_title_stage_and_objectives
    Quests.start(:well)
    assert_equal %i[section quest stage objective objective], kinds
    assert_equal ["errand.title (tracked)", "errand.go", "errand.well", "errand.water"], lines.drop(1).map(&:text)
    assert(lines.drop(1).all? { |line| line.quest.name == "well" })
  end

  def test_an_objective_says_whether_it_is_done
    Quests.start(:well)
    Quests.collected(:water, 2)
    objectives = lines.select { |line| line.kind == :objective }
    assert_equal [false, true], objectives.map(&:done)
  end

  def test_completed_quests_come_last_under_their_own_caption
    Quests.start(:well)
    Quests.start(:mill)
    Quests.set_stage(:mill, "done")
    assert_equal %i[section quest stage objective objective section completed], kinds
    assert_equal %w[Completed errand.title], lines.last(2).map(&:text)
    assert lines.last.done
  end

  def test_the_first_quest_under_way_is_tracked_unless_another_is_picked
    assert_nil Quests.tracked
    Quests.start(:well)
    Quests.start(:mill)
    assert_equal "well", Quests.tracked.name
    Quests.track(:mill)
    assert_equal "mill", Quests.tracked.name
    titles = lines.select { |line| line.kind == :quest }.map(&:text)
    assert_equal ["errand.title", "errand.title (tracked)"], titles
  end

  def test_a_quest_no_longer_under_way_is_no_longer_tracked
    Quests.start(:well)
    Quests.start(:mill)
    Quests.track(:mill)
    Quests.set_stage(:mill, "done")
    assert_equal "well", Quests.tracked.name
  end

  def test_its_lines_take_the_focus_and_its_back_button_goes_to_the_pause_menu
    Quests.start(:well)
    shown = menu(journal: true)
    shown.show(:journal)
    journal = shown.screens[:journal]
    assert_equal ["Journal", lines.size, ["Back"]], [journal.heading, journal.rows, journal.options.map(&:first)]
    journal.back.call
    assert_equal :pause, shown.screen
  end

  def test_picking_a_line_of_a_quest_tracks_it
    Quests.start(:well)
    Quests.start(:mill)
    journal = focused_journal(lines.index { |line| line.kind == :quest && line.quest.name == "mill" })
    UI.pick_focused = true
    journal.draw(FIRST_BUTTON)
    UI.pick_focused = false
    assert_equal "mill", Quests.tracked.name
  end

  def test_a_stage_with_no_text_has_no_line
    Quests.instance_variable_get(:@definitions)["mute"] = Quests::Definition.new("mute", UNTOLD)
    Quests.start(:mute)
    assert_equal %i[section quest objective], kinds
  end

  def test_it_goes_before_quit_even_after_an_option_the_game_added_last
    shown = GameMenu.new
    shown.__configure({ title: "Errands", start_open: false, journal: true })
    shown.screens[:pause].add_option("Credits") { :credits }
    shown.start
    assert_equal %w[Resume Restart Settings Journal Quit Credits], shown.screens[:pause].options.map(&:first)
  end

  def test_the_pause_menu_offers_it_before_quit
    assert_equal %w[Resume Restart Settings Journal Quit], menu(journal: true).screens[:pause].options.map(&:first)
    assert_equal %w[Resume Restart Settings Quit], menu(journal: false).screens[:pause].options.map(&:first)
  end

  def test_it_draws_every_kind_of_line
    Quests.start(:well)
    Quests.start(:mill)
    Quests.set_stage(:mill, "done")
    journal = focused_journal(0)
    journal.draw(FIRST_BUTTON)
    assert_equal 7, journal.rows
  end
end
