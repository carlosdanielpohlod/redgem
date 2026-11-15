# Turns an NPC's head toward who is near it, on top of what its clips do (see Animator#look_at):
# the nearest of the players' heroes in range and in front of it, or what the game says.
#
#   npc.add_component(HeadLook, range: 5.0)
#   npc.get_component(HeadLook).target = find("Shopkeeper")   # an object or a point; nil: the heroes
#
# Settings (each also a Blender property of the object): bone (the name of the head's bone: "Head"
# unless said; a rig's prefix, as in "mixamorig:Head", is fine), range (meters to a hero), angle
# (degrees off its forward a hero may be at, and the most the head turns), aim (meters above a
# hero's origin it looks at: its eyes). It runs on every machine: each sees the heads turn toward
# the heroes it sees.
class HeadLook < Behaviour
  RANGE = 4.0     # meters
  ANGLE = 70.0    # degrees
  AIM = 1.6       # meters above a hero's origin

  # What it looks at instead of the heroes: an object, a Vector3, or nil.
  attr_accessor :target

  def start = bone  # a missing bone stops here

  def update(dt)
    return if dt.zero?  # paused: the head holds still

    looked = target || nearest_hero
    return animation.look_away(bone: bone) if looked.nil?

    animation.look_at(point_of(looked), bone: bone, max_angle: angle)
  end

  # Its head goes back to its pose with it.
  def on_destroy
    animation.look_away(bone: @bone) if @bone&.alive?
  end

  # The head's bone (an object under this one).
  def bone = @bone ||= animation.bone!(prop(:bone, Animator::HEAD))

  def range = Float(prop(:range, RANGE))
  def angle = Float(prop(:angle, ANGLE))

  # The nearest hero in range and in front of it, or nil.
  def nearest_hero
    near = Interactor.all.map(&:game_object).select { |hero| in_view?(hero) }
    near.min_by { |hero| Interaction.distance(game_object, hero) }
  end

  private

  def in_view?(hero)
    hero != game_object && Interaction.distance(game_object, hero) <= range &&
      Interaction.angle_to(game_object, hero) <= angle
  end

  def point_of(looked)
    looked.is_a?(Vector3) ? looked : looked.transform.position + Vector3.up * Float(prop(:aim, AIM))
  end
end
