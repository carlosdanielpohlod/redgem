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
  RESET_KEY = :r

  def start
    @spawn_position = transform.position
    @spawn_rotation = transform.rotation
  end

  def update(_dt)
    handbrake = Input.action?(:jump) ? FULL : NONE
    vehicle.drive(steering: Input.axis(:horizontal), handbrake: handbrake, **pedals(Input.axis(:vertical)))
    reset if reset_asked?
  end

  def speed_kmh = vehicle.speed * MS_TO_KMH

  # Back to the start line, upright and stopped.
  def reset
    transform.rotation = @spawn_rotation
    transform.position = @spawn_position + Vector3.up * RESET_LIFT
  end

  private

  # By the driver, or by a car that fell off the track.
  def reset_asked? = Input.pressed?(RESET_KEY) || transform.position.y < FALL_LIMIT

  # The throttle and brake for what the keys ask (-1 to 1).
  def pedals(throttle)
    return { throttle: NONE, brake: FULL } if braking?(throttle)

    { throttle: throttle, brake: throttle.zero? ? COAST_BRAKE : NONE }
  end

  # Pressing against the way the car is going brakes it, until it is slow enough to reverse.
  def braking?(throttle) = (throttle * vehicle.speed).negative? && vehicle.speed.abs > REVERSE_THRESHOLD
end
