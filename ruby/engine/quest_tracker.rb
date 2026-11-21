# Follows the quests for the object it is on, the player's hero (see quests.rb). Every frame it
# tells the quests when the hero comes within reach of a go_to objective's place (an object of that
# name, within the objective's "radius"), and, where the quests are decided (offline, on the host),
# asks the game's checks (Quests.check). On this machine's hero it also shows where the tracked
# quest's objectives happen (see QuestMarkers).
#
#   find("Player").add_component(QuestTracker)
#
# On another machine's hero it does not look for places nor show them: that machine's own tracker
# does, and tells the host. A dedicated server, which has no hero, needs one on any object for its checks.
class QuestTracker < Behaviour
  def update(dt)
    Quests.__poll
    return unless mine?

    look_around
    QuestMarkers.__update(transform.position, dt)
  end

  private

  # (A hero with no NetworkSync is this machine's.)
  def mine? = get_component(NetworkSync)&.mine? != false

  def look_around
    waiting = waiting_arrivals
    @reported = reported & waiting.map { |objective| report_key(objective) }
    objective = waiting.find { |entry| ready_to_report?(entry) }
    # Reporting can change stages. Re-read the objectives next frame.
    reach(objective) if objective
  end

  def waiting_arrivals = Quests.active.flat_map(&:objectives).select { |objective| awaiting_arrival?(objective) }

  def ready_to_report?(objective) = !reported?(objective) && near?(objective)

  def awaiting_arrival?(objective) = objective.kind == :go_to && !objective.done?

  def near?(objective)
    place = Scene.find(objective.target)
    !place.nil? && transform.position.distance_to(place.transform.position) <= objective.radius
  end

  # (On a client, the host's answer comes a moment later: each place is told of once a stage.)
  def reported?(objective) = reported.include?(report_key(objective))

  def reach(objective)
    key = report_key(objective)
    reported << key if Quests.reached(objective.target)
  end

  def reported = @reported ||= []

  def report_key(objective) = [objective.quest.name, objective.quest.stage, objective.index]
end
