# What the player reads on screen: the controls, who they play with, and the quest's objectives
# (the texts are in locales/en.json).
class Hud < Behaviour
  X = 20
  LINE = 30           # pixels from one line to the next
  SIZE = 22
  QUEST_COLOR = :gold
  TOP_LINES = 2       # the controls and the network: white, then the quest's lines

  def update(_dt)
    lines = [I18n.t("hud.controls"), network_line] + quest_lines
    lines.each_with_index { |text, row| UI.text(text, x: X, y: X + row * LINE, size: SIZE, color: color(row)) }
  end

  private

  def color(row) = row < TOP_LINES ? :white : QUEST_COLOR

  def network_line
    return I18n.t("hud.offline") unless Network.online?

    others = Network.players.size
    return I18n.t("hud.online", count: others) unless Network.host?

    I18n.t("hud.hosting", port: Network.port, code: Network.host_code.to_s, count: others)
  end

  # The objectives of every quest under way, and the quests done.
  def quest_lines
    active = Quests.active.flat_map { |quest| quest.objectives.map(&:text) }
    active + Quests.completed.map { |quest| I18n.t("hud.complete", title: quest.title) }
  end
end
