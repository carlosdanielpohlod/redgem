# What the editor draws over a selected object while the game runs in it (src/editor/npc_gizmo.hpp):
# the settings of the object's Interactable, Perception, Schedule and NavAgent, in one Hash. The
# engine asks for it; a game has no use for it.
module EditorGizmos
  PARTS = { Interactable => :interactable, Perception => :perception, Schedule => :schedule,
            NavAgent => :nav_agent }.freeze

  class << self
    # { range:, prompt:, sight:, fov:, eyes:, hearing:, plan:, destination:, waypoints: },
    # with the keys of the parts the object has.
    def __settings(object)
      PARTS.each_with_object({}) do |(klass, part), settings|
        component = object.get_component(klass)
        settings.merge!(send(part, component)) if component
      end
    end

    private

    def interactable(part) = { range: part.range, prompt: part.prompt }

    def perception(part) = { sight: part.sight, fov: part.fov, eyes: part.eyes, hearing: part.hearing }

    def schedule(part) = { plan: part.plan.map { |minute, activity| "#{clock(minute)} #{activity}" } }

    def nav_agent(part) = part.destination ? { destination: part.destination, waypoints: part.waypoints.dup } : {}

    def clock(minute) = format(GameClock::CLOCK_FORMAT, *minute.divmod(GameClock::MINUTES_PER_HOUR))
  end
end
