# How an object someone else owns is shown: the poses its owner sent are kept a short while, and
# the object is shown a little behind the newest of them, blended between the two around that
# moment. It moves smoothly however unevenly they arrive; when the next one is late it goes on a
# little the way it was going, then waits. The delay follows how evenly poses arrive: steady
# arrivals keep it short, wandering ones make it longer. Times are seconds on the watcher's clock,
# counted from when each pose arrived (the owner sends no time: any version of the game can be it).
class NetworkSync < Behaviour
  class Snapshots
    KEPT = 32                 # poses held at most
    INTERVALS_BEHIND = 1.5    # how far behind the newest pose an object is shown, in sends ...
    JITTER_MARGIN = 2.0       # ... and in how much the time between arrivals wanders, this many times over
    MIN_DELAY = 0.05          # seconds an object is shown behind at least, and at most
    MAX_DELAY = 0.35
    ADAPTING = 0.1            # how much one arrival moves what is known of the others (0 to 1)
    LONGEST_GAP = 0.5         # seconds between two arrivals past which it was a pause, not the pace
    MAX_EXTRAPOLATION = 0.2   # seconds an object goes on past its newest pose when the next one is late

    Pose = Struct.new(:time, :position, :rotation)

    # `interval`: the seconds expected between poses. `delay`: seconds to show the object behind,
    # always (nil: as the arrivals say).
    def initialize(interval, delay = nil)
      @clock = 0.0
      @poses = []
      @animations = [] # [when it arrived, what the owner's animation did]
      @interval = interval
      @jitter = 0.0
      @fixed_delay = delay
    end

    def advance(dt) = @clock += dt

    # How far behind its newest pose the object is shown now.
    def delay
      @fixed_delay || (@interval * INTERVALS_BEHIND + @jitter * JITTER_MARGIN).clamp(MIN_DELAY, MAX_DELAY)
    end

    # Where the owner says the object is, as of now. Far from where it was, it was moved there at
    # once (a teleport): the poses before are forgotten, so it is never shown sliding across.
    # An animation that came along is done when the object is shown at that moment.
    def add(position, rotation, animation = nil)
      newest = @poses.last
      make_way(newest, position) if newest
      go_on_from_shown(@clock - delay)
      @poses << Pose.new(@clock, position, rotation)
      @poses.shift while @poses.size > KEPT
      @animations << [@clock, animation] if animation
      @animations.shift while @animations.size > KEPT
    end

    # [animation, seconds since its moment] for the newest animation whose moment the object is
    # shown at by now (the older ones are passed over); nil when there is none.
    def due_animation
      moment = @clock - delay
      due = nil
      due = @animations.shift while @animations.first && @animations.first[0] <= moment
      due && [due[1], moment - due[0]]
    end

    # [position, rotation] to show the object at now; nil before any pose arrived.
    def pose
      return if @poses.empty?

      moment = @clock - delay
      drop_older_than(moment)
      shown = pose_at(moment)
      [shown.position, shown.rotation]
    end

    private

    # Keeps the two poses around `moment`, or the last two before it.
    def drop_older_than(moment)
      @poses.shift while @poses.size > 2 && @poses[1].time <= moment
    end

    # Past its newest pose, the object went on the way it was going: a pose that comes late takes
    # it on from where it is shown, never back to where it was.
    def go_on_from_shown(moment)
      return unless gone_on?(moment)

      drop_older_than(moment)
      shown = pose_at(moment)
      @poses = [Pose.new(moment, shown.position, shown.rotation)]
    end

    def gone_on?(moment) = @poses.size > 1 && moment > @poses.last.time

    def make_way(newest, position)
      learn(@clock - newest.time)
      return @poses.clear if newest.position.distance_to(position) > SNAP_DISTANCE

      @poses.pop if newest.time == @clock # two in one frame: the newer is the one
    end

    # What the time between arrivals says of the pace, and of how much it wanders.
    def learn(gap)
      return unless gap.positive? && gap <= LONGEST_GAP

      @jitter += ((gap - @interval).abs - @jitter) * ADAPTING
      @interval += (gap - @interval) * ADAPTING
    end

    def pose_at(moment)
      first, second = @poses
      return first if second.nil? || moment <= first.time

      until_then = [moment, second.time + MAX_EXTRAPOLATION].min
      blend(first, second, (until_then - first.time) / (second.time - first.time))
    end

    # Part of the way from one pose to the next; past it (more than 1), on the way it was going.
    def blend(from, to, share)
      angles = from.rotation.to_a.zip(to.rotation.to_a)
      rotation = Vector3[*angles.map { |a, b| NetworkSync.toward(a, b, share) }]
      Pose.new(nil, from.position.lerp(to.position, share), rotation)
    end
  end
end
