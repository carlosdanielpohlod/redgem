# The scene's animation clips, by the name they have in Blender. All of them play in a loop from
# the start; this is for the ones that should not: a door that opens once, a lift that waits.
#
#   Animation.stop(:DoorOpen)                 # in a scene script's setup: the door starts closed
#   Animation.play(:DoorOpen, loop: false)    # opens and stays open
#   Animation.play(:DoorOpen, loop: false, speed: -1.0)   # closes
module Animation
  NORMAL_SPEED = 1.0

  # Carries on from where the clip is; restart: true plays it from the beginning.
  def self.play(name, loop: true, speed: NORMAL_SPEED, restart: false)
    __play(name, loop, speed.to_f, restart)
  end
end

# One character's own animation: `game_object.animation`. A character is an object and what is
# under it, or a spawned copy as a whole. Until it is told to play something it follows the
# scene's clips like everything else.
#
#   animation.play(:Run, fade: 0.2)      # from whatever it was doing to running, over 0.2 s
#   animation.play(:Wave, loop: false)   # once, holding the last pose
#   animation.mix(:Limp, weight: 0.5)    # half of a second clip laid over all that
#   animation.follow_scene(fade: 0.3)    # back to what the level plays
class Animator
  NO_FADE = 0.0
  WHOLE = 1.0

  def initialize(game_object)
    @game_object = game_object
  end

  # Playing the clip it is already on carries on (restart: true starts it over).
  def play(name, loop: true, speed: Animation::NORMAL_SPEED, fade: NO_FADE, restart: false)
    @game_object.__animation_play(name, loop, speed.to_f, fade.to_f, restart)
  end

  # Lays a second clip over what the character is doing, by `weight` (0 to 1). A clip that only
  # moves some bones (a wave of the arm) only changes those: the rest keeps walking.
  def mix(name, weight: WHOLE, loop: true, speed: Animation::NORMAL_SPEED)
    @game_object.__animation_mix(name, weight.to_f, loop, speed.to_f)
  end

  def unmix = @game_object.__animation_unmix
  def pause = @game_object.__animation_pause
  def follow_scene(fade: NO_FADE) = @game_object.__animation_follow_scene(fade.to_f)

  # The name of its own clip, or nil while it follows the scene's.
  def clip = @game_object.__animation_clip
  def playing? = @game_object.__animation_playing
  def time = @game_object.__animation_time
end

class GameObject
  def animation = @animation ||= Animator.new(self)
end
