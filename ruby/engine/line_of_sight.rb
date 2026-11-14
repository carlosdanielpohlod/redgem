# Whether one thing can see another: a ray from a point to a target hits nothing but the target
# (or what is under it) first. An Interactor uses it to reach what it uses, a Perception to see.
#
#   LineOfSight.clear?(eyes, player.transform.position, player, ignore: LineOfSight.carried(game_object))
module LineOfSight
  # Meters short of a target a ray may stop (on the target's own surface) and still see it.
  TOLERANCE = 0.05
  NEAR = 1e-4   # meters: closer than this, the target is where the ray starts

  class << self
    # Whether a ray from `from` to `to` (points) reaches `target` (an object) before anything else.
    # ignore: objects the ray sees through.
    def clear?(from, to, target, ignore: [])
      length = from.distance_to(to)
      return true if length < NEAR

      hit = Physics.raycast(from, (to - from) / length, length, ignore: ignore)
      unblocked?(hit, target, length)
    end

    # An object and everything under it: what it carries hides nothing from it.
    def carried(object) = [object] + object.children.flat_map { |child| carried(child) }

    # Whether an object is the whole, or is under it (a character's collider on a child).
    def part_of?(object, whole)
      object = object.parent until object.nil? || object == whole
      !object.nil?
    end

    private

    def unblocked?(hit, target, length)
      hit.nil? || hit[:distance] >= length - TOLERANCE || part_of?(hit[:object], target)
    end
  end
end
