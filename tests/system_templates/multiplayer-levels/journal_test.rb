# A game just made from the multiplayer-levels template shows its quest to the player: once the
# guide waits to hear of the beacon, a marker (or, off the screen, a pointer at its edge) shows
# where the guide is, and the pause menu's journal lists the quest with its stage and objective.
# The journal is opened with a click, worked with a gamepad, and left with Esc.
SETTLE_FRAMES = 30
PLAY_BUTTON = 0
MARK_FRAMES = 5
EDGE = 1.0                # units a pointer may be off its margin

SystemTest.define do |t|
  Progress.clear
  t.wait(SETTLE_FRAMES)
  menu = GameMenu.instance
  menu.screens[:title].options[PLAY_BUTTON][1].call
  Quests.start(:beacon)
  Quests.set_stage(:beacon, "report")
  t.wait(MARK_FRAMES)

  t.assert("the guide, whom the quest waits for, is marked") do
    QuestMarkers.marks.map { |mark| mark.objective.target } == ["Guide"]
  end
  t.assert("its mark is on the screen, or a pointer at the margin of its edge") do
    mark = QuestMarkers.marks.first
    from_edge = [mark.x, UI.width - mark.x, mark.y, UI.height - mark.y].min
    at_margin = (from_edge - QuestMarkers::STYLE[:margin]).abs < EDGE
    mark.pointer ? at_margin : QuestMarkers.on_screen?([mark.x, mark.y], UI.width, UI.height)
  end
  t.screenshot("quest_marker")

  t.press_button(:start)
  t.wait
  t.assert("Start opens the pause menu") { menu.screen == :pause }
  journal_button = menu.screens[:pause].options.index { |label, _action| label == I18n.t("game.menu.journal") }
  t.assert("the pause menu offers the journal") { !journal_button.nil? }
  x, y, width, height = menu.button_box(journal_button)
  t.click(x + width / 2, y + height / 2)
  t.assert("a click opens the journal") { menu.screen == :journal }
  journal = menu.screens[:journal]
  t.assert("it lists the quest, tracked, with its stage and objective") do
    texts = journal.lines.map(&:text)
    ["The beacon (tracked)", "Tell the guide what you found", "Talk to the guide"].all? { |text| texts.include?(text) }
  end
  t.screenshot("journal")

  t.press_button(:dpad_down)
  t.wait
  t.assert("the D-pad moves the focus down its lines") { journal.focus.index == 1 }
  t.press_button(:a)
  t.wait
  t.assert("A on the quest's line keeps it tracked") { Quests.tracked.name == "beacon" }
  t.press(:escape)
  t.wait
  t.assert("Esc goes back to the pause menu") { menu.screen == :pause }
end
