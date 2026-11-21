# What the menu's journal shows (see GameMenu::JournalScreen), apart from how it is drawn: its
# lines, in order. Under "Under way": each quest's title (the tracked one says so), the text of the
# stage it is at, and its objectives, each done or not; or a line that says none is. Under
# "Completed", when one is: the title of each quest done. The texts are in the language in use.
# The quests are the party's: on a client, the host's, as it told them.
class GameMenu < Behaviour
  class Journal
    # One line: what it is (:section, :quest, :stage, :objective, :none or :completed), its text,
    # whether it is done (an objective done, a quest completed), and the quest it is of (nil for a
    # section's caption and for :none).
    Line = Struct.new(:kind, :text, :done, :quest)

    # The lines of these quests (those under way, those completed, and the one tracked).
    def self.lines(active = Quests.active, completed = Quests.completed, tracked = Quests.tracked)
      new(tracked).lines(active, completed)
    end

    def initialize(tracked)
      @tracked = tracked
    end

    def lines(active, completed) = under_way(active) + done(completed)

    private

    def under_way(active)
      caption = Line.new(:section, I18n.t("game.journal.active"), false, nil)
      return [caption, Line.new(:none, I18n.t("game.journal.none"), false, nil)] if active.empty?

      [caption] + active.flat_map { |quest| quest_lines(quest) }
    end

    def done(completed)
      return [] if completed.empty?

      [Line.new(:section, I18n.t("game.journal.completed"), false, nil)] +
        completed.map { |quest| Line.new(:completed, quest.title.to_s, true, quest) }
    end

    # A quest's title, its stage's text (a stage may have none) and its objectives.
    def quest_lines(quest)
      stage = quest.stage_text
      lines = [Line.new(:quest, title_of(quest), false, quest)]
      lines << Line.new(:stage, stage, false, quest) if stage
      lines + quest.objectives.map { |objective| Line.new(:objective, objective.text, objective.done?, quest) }
    end

    def title_of(quest)
      tracked?(quest) ? I18n.t("game.journal.tracked", title: quest.title.to_s) : quest.title.to_s
    end

    def tracked?(quest) = !@tracked.nil? && quest == @tracked
  end
end
