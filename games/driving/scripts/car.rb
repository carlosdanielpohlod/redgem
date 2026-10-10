# Player car: W/S throttle and reverse, A/D steer, Space hand brake, R puts it back on the road.
class Car < Behaviour
  MS_TO_KMH = 3.6
  FALL_LIMIT = -10.0
  RESET_LIFT = 1.0
  # Below this speed (m/s) pressing the opposite direction reverses instead of braking.
  REVERSE_THRESHOLD = 1.0
  FULL = 1.0
  NONE = 0.0
  # Light brake while coasting: slows the car down and stops idle creep.
  COAST_BRAKE = 0.2

  def start
    @spawn_position = transform.position
    @spawn_rotation = transform.rotation
  end

  def update(_dt)
    throttle = Input.axis(:vertical)
    brake = throttle.zero? ? COAST_BRAKE : NONE
    if throttle * vehicle.speed < 0 && vehicle.speed.abs > REVERSE_THRESHOLD
      brake = FULL
      throttle = NONE
    end
    handbrake = Input.action?(:jump) ? FULL : NONE
    vehicle.drive(throttle: throttle, steering: Input.axis(:horizontal), brake: brake, handbrake: handbrake)
    reset if Input.pressed?(:r) || transform.position.y < FALL_LIMIT
  end

  def speed_kmh = vehicle.speed * MS_TO_KMH

  # Back to the start line, upright and stopped.
  def reset
    transform.rotation = @spawn_rotation
    transform.position = @spawn_position + Vector3.up * RESET_LIFT
  end
end
