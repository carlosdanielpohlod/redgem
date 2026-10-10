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
    breath = Math.sin(time / BREATH_SECONDS * FULL_TURN + @phase) * BREATH_HEIGHT
    look = Math.sin(time / @pace * FULL_TURN + @phase) * @look
    lean = Math.sin(time / (@pace * LEAN_PACE) * FULL_TURN + @phase) * LEAN_DEGREES
    transform.position = Vector3.new(@position.x, @position.y + breath, @position.z)
    transform.rotation = Vector3.new(lean, @yaw + look, 0)
  end
end
