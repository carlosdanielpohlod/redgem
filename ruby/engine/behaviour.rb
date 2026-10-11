# Base class for game scripts: one instance per object it is attached to.
#
#   class Player < Behaviour
#     def start; end
#     def update(dt); end
#     def fixed_update(dt); end
#     def on_trigger_enter(other); end
#     def on_trigger_exit(other); end
#     def on_collision_enter(other); end
#     def on_collision_exit(other); end
#     def on_destroy; end
#   end
class Behaviour
  attr_reader :game_object

  def start; end
  def update(_dt); end
  def fixed_update(_dt); end
  def on_trigger_enter(_other); end
  def on_trigger_exit(_other); end
  def on_collision_enter(_other); end
  def on_collision_exit(_other); end
  def on_destroy; end

  def transform = game_object.transform
  def character = game_object.character
  def rigidbody = game_object.rigidbody
  def vehicle = game_object.vehicle
  def name = game_object.name

  # Blender custom properties of this object, with symbol keys.
  def props = game_object.props

  # A setting of this behaviour: what add_component was given, else the object's Blender
  # property of that name, else the default.
  def prop(key, default = nil)
    value = @settings[key.to_sym] if @settings
    value = props[key.to_sym] if value.nil?
    value.nil? ? default : value
  end

  def find(name) = Scene.find(name)
  def find_all(tag) = Scene.find_all(tag)
  def destroy(target = game_object) = target.destroy
  def get_component(klass) = game_object.get_component(klass)
  # This object's own animation (see Animator): animation.play(:Run, fade: 0.2).
  def animation = game_object.animation

  # --- Time: things that happen later, or over a while ------------------------------------------
  # All of it runs on game time: nothing fires while the game is paused (Time.scale = 0), and a
  # behaviour's timers end with it.

  # Runs the block once, `seconds` from now. Returns a timer: call `cancel` on it to call it off.
  #   after(2.0) { destroy }
  def after(seconds, &block) = __add_timer(Timer.new(seconds, false, block))

  # Runs the block every `seconds`, until cancelled.
  #   every(0.5) { Scene.spawn("Bullet", position: transform.position) }
  def every(seconds, &block) = __add_timer(Timer.new(seconds, true, block))

  # Calls the block every frame for `seconds` with how far along it is, from 0 to 1 (ending exactly
  # on 1). ease: :smooth starts and ends gently, :linear goes at a steady pace.
  #   tween(0.4) { |t| transform.scale = Vector3.one * (1.0 + t) }
  def tween(seconds, ease: :smooth, &block)
    raise ArgumentError, "unknown ease #{ease.inspect} (use one of #{EASES.inspect})" unless EASES.include?(ease)

    block.call(0.0)  # the starting pose, right away
    __add_timer(Tween.new(seconds, ease, block))
  end

  # Calls off every timer of this behaviour (also from inside one of them).
  def cancel_timers
    @timers&.each(&:cancel)
    @running_timers&.each(&:cancel)
  end

  EASES = [:smooth, :linear].freeze

  # Something waiting to happen (see after, every).
  class Timer
    MAX_RUNS_PER_FRAME = 8   # of an `every` shorter than a frame; what it missed beyond that is dropped

    def initialize(seconds, repeats, block)
      @seconds = seconds.to_f
      @repeats = repeats
      @block = block
      @waited = 0.0
      @cancelled = false
    end

    def cancel = @cancelled = true
    def cancelled? = @cancelled

    # Advances by dt seconds; returns true once it has nothing left to do.
    def __advance(dt)
      return true if @cancelled

      @waited += dt
      fired = 0
      while @waited >= @seconds
        @block.call
        return true unless @repeats && !@cancelled

        @waited -= @seconds
        fired += 1
        # every(0) runs once a frame, and a tiny interval a few times: never forever.
        next if @seconds.positive? && fired < MAX_RUNS_PER_FRAME

        @waited = 0.0
        break
      end
      false
    end
  end

  # Something happening over a while (see tween).
  class Tween < Timer
    def initialize(seconds, ease, block)
      super(seconds, false, block)
      @ease = ease
    end

    def __advance(dt)
      return true if @cancelled

      @waited += dt
      progress = @seconds.positive? ? [@waited / @seconds, 1.0].min : 1.0
      @block.call(@ease == :linear ? progress : progress * progress * (3.0 - 2.0 * progress))
      progress >= 1.0
    end
  end

  def inspect = "#<#{self.class} on #{game_object.inspect}>"

  def __configure(settings)
    @settings = settings
  end

  def __add_timer(timer)
    (@timers ||= []) << timer
    timer
  end

  # What the engine calls every frame: timers first, then the behaviour's own update.
  def __update(dt)
    if @timers && !@timers.empty? && dt.positive?
      # Timers added while these run wait for the next frame.
      running = @running_timers = @timers
      @timers = []
      running.reject! { |timer| timer.__advance(dt) }
      @running_timers = nil
      @timers = running + @timers
    end
    update(dt)
  end

  def __attach(game_object)
    @game_object = game_object
    game_object.__add_behaviour(self)
  end
end
