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

  # Playing the clip it is already on carries on (restart: true starts it over). root_motion: true
  # lets this clip move the character (false stops it; see root_motion=).
  def play(name, loop: true, speed: Animation::NORMAL_SPEED, fade: NO_FADE, restart: false, root_motion: nil)
    @game_object.__animation_clip_root_motion(name, root_motion ? true : false) unless root_motion.nil?
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

# Animation events and root motion.
#
#   animation.on(:Attack, at: 0.4) { |event| hit_what_is_ahead }   # 40% into the clip
#   animation.on(:Walk, seconds: 0.3) { Audio.play(:step) }
#   animation.on_marker(:footstep) { Audio.play(:step) }   # markers authored in Blender
#   animation.root_motion = true                           # its clips move the character
class Animator
  # Calls the block (with an Animation::Event) each time the clip passes a moment of it: at: from 0
  # (its start) to 1 (its end), or seconds: into it. Returns what off takes.
  def on(clip, at: nil, seconds: nil, &block)
    needs_a_block(block)
    remember(@game_object.__animation_listen_at(clip, moment(clip, at, seconds)), block)
  end

  # Calls the block each time any of its clips passes a marker called `name` that the clip was
  # exported with (see the redgem_events custom property in animation.md).
  def on_marker(name, &block)
    needs_a_block(block)
    remember(@game_object.__animation_listen_marker(name), block)
  end

  def off(listener)
    @game_object.__animation_unlisten(listener)
    listeners.delete(listener)
    nil
  end

  # Whether all its clips move the character by their root's motion (play's root_motion: picks clips).
  def root_motion=(enabled)
    @game_object.__animation_set_root_motion(enabled ? true : false)
  end

  def root_motion? = @game_object.__animation_root_motion

  def __fire(listener, event)
    listeners[listener]&.call(event)
  end

  private

  def listeners = @listeners ||= {}

  def needs_a_block(block)
    raise ArgumentError, "an animation event needs a block" unless block
  end

  def remember(listener, block)
    listeners[listener] = block
    listener
  end

  def moment(clip, at, seconds)
    raise ArgumentError, "an animation event takes either at: or seconds:" unless at.nil? ^ seconds.nil?
    return at.to_f if seconds.nil?

    length = Animation.duration(clip)
    length.positive? ? seconds.to_f / length : 0.0
  end
end

module Animation
  # What the block of an animation event is given (Animator#on): the object that listened, the
  # clip, the marker's name (nil for a moment the script named) and where in the clip it is, 0 to 1.
  class Event
    attr_reader :game_object, :clip, :name, :at

    def initialize(game_object, clip, name, at)
      @game_object = game_object
      @clip = clip
      @name = name
      @at = at
    end
  end
end

class GameObject
  def animation = @animation ||= Animator.new(self)

  def __animation_event(listener, clip, name, at)
    animation.__fire(listener, Animation::Event.new(self, clip, name, at))
  end
end
