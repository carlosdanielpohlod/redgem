# A car's body on its springs: it squats when the car accelerates, dives when it brakes and rolls
# out of turns, settling gently instead of snapping.
class BodySprings
  SQUAT_PER_ACCELERATION = 0.16   # degrees of pitch per m/s^2 (nose up accelerating, down braking)
  MAX_PITCH = 3.2                 # degrees
  ROLL_PER_TURN = 0.11            # degrees of roll per TURN_UNIT of turning (degrees per second x m/s)
  TURN_UNIT = 10.0
  MAX_ROLL = 4.5                  # degrees
  QUICKNESS = 6.0                 # how fast the body settles (per second)

  # Degrees the body leans right now: forward and back, and to the side.
  attr_reader :pitch, :roll

  def initialize
    @pitch = 0.0
    @roll = 0.0
  end

  # Called every frame with the car's acceleration (m/s^2), how fast it is turning (degrees per
  # second) and its speed (m/s).
  def lean(acceleration, turning, speed, dt)
    settle = [QUICKNESS * dt, 1.0].min
    pitch = (-acceleration * SQUAT_PER_ACCELERATION).clamp(-MAX_PITCH, MAX_PITCH)
    roll = (turning * speed * ROLL_PER_TURN / TURN_UNIT).clamp(-MAX_ROLL, MAX_ROLL)
    @pitch += (pitch - @pitch) * settle
    @roll += (roll - @roll) * settle
  end
end
