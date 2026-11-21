# The camera behind a menu's screen flying a path (its theme's background camera_path): through
# its points and back to the first, in `seconds` a lap, easing into and out of each, turned to look
# at `look_at` when there is one. It moves by real time (the game is frozen behind the menu), as
# what moves the camera (a Camera::Rig's motion) for as long as the screen shows, and then gives
# the camera back where it was, to what moved it before.
class GameMenu < Behaviour
  class CameraPath
    def initialize(path, motion_was, transform)
      @path = path
      @motion_was = motion_was
      @seat_was = [transform.position, transform.rotation]
      @time = 0.0
    end

    # Whether it flies that path (a screen with the same one keeps it flying on).
    def flies?(path) = @path.equal?(path) || @path == path

    def lost? = false

    def update(transform, _dt, real_dt)
      @time += real_dt
      transform.position = at(@time)
      Camera.__look(transform, @path[:look_at]) if @path[:look_at]
    end

    # Where it is `time` seconds into its flight.
    def at(time)
      points = @path[:points]
      return points.first if points.size == 1

      lap = (time % @path[:seconds]) / @path[:seconds] * points.size
      from = lap.floor
      points[from].lerp(points[(from + 1) % points.size], smooth(lap - from))
    end

    # The camera back where it was, and to what moved it.
    # (Unless something else took the camera meanwhile: Camera.follow, say.)
    def give_back(rig, transform)
      return unless rig&.motion.equal?(self)

      rig.motion = @motion_was
      seat(transform) if transform
    end

    private

    def seat(transform)
      transform.position = @seat_was.first
      transform.rotation = @seat_was.last
    end

    # Slow at both ends of a stretch, fastest in its middle (smoothstep).
    def smooth(share) = share * share * (3.0 - 2.0 * share)
  end
end
