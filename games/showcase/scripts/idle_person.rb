# A person standing in place: breathes and slowly looks around, so they do not read as statues.
# Settings: look (degrees to each side), pace (seconds per look cycle).
class IdlePerson < Behaviour
  DEFAULT_LOOK = 5.0
  DEFAULT_PACE = 7.0
  BREATH_HEIGHT = 0.006   # meters
  BREATH_SECONDS = 3.5
  LEAN_DEGREES = 0.8
  FULL_TURN = 2.0 * Math::PI
  DEGREES_PER_RADIAN = 180.0 / Math::PI
  PHASE_PER_METER = 1.7   # people at different places move out of step
  LEAN_PACE = 0.5         # the lean cycles this fraction of the look cycle (twice as fast)

  def start
    @position = transform.position
    forward = transform.forward
    @yaw = Math.atan2(-forward.x, -forward.z) * DEGREES_PER_RADIAN
    @look = prop(:look, DEFAULT_LOOK)
    @pace = prop(:pace, DEFAULT_PACE)
    @phase = (@position.x + @position.z) * PHASE_PER_METER
  end

  def update(_dt)
    time = Time.elapsed
    breath = sway(time, BREATH_SECONDS, BREATH_HEIGHT)
    look = sway(time, @pace, @look)
    lean = sway(time, @pace * LEAN_PACE, LEAN_DEGREES)
    transform.position = @position.with(y: @position.y + breath)
    transform.rotation = Vector3.new(lean, @yaw + look, 0)
  end

  private

  # Something that goes back and forth by up to `size`, once every `seconds`.
  def sway(time, seconds, size) = Math.sin(time / seconds * FULL_TURN + @phase) * size
end
