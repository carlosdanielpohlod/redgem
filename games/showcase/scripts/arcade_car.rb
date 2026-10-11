# Arcade driving for a car without wheel physics: W/S accelerate and brake (then reverse),
# A/D steer, Space is a hard brake. With a gamepad: the triggers accelerate and brake, the left
# stick steers, A is the hard brake. The body leans into what the car is doing (BodySprings), the
# engine is heard climbing through its gears (EngineSound), and a speedometer is drawn
# (Speedometer). The car stays inside the street, cannot drive through other cars (it needs a
# CarBody on the same object for that) and drops into potholes (Potholes).
# Settings: reversed (the model's nose points along -forward), min_x, max_x, min_z, max_z (street
# bounds in meters).
class ArcadeCar < Behaviour
  MAX_SPEED = 30.0          # m/s, 108 km/h
  MAX_REVERSE_SPEED = 6.0
  ACCELERATION = 8.5        # m/s^2 at a standstill ...
  TOP_END_SHARE = 0.35      # ... easing to this share of it near top speed
  BRAKING = 16.0
  HARD_BRAKING = 26.0
  ROLLING_DRAG = 2.0
  STEER_DEGREES_PER_SECOND = 70.0
  STEERING_QUICKNESS = 5.0  # how fast the wheel follows the keys (per second)
  FULL_STEER_SPEED = 6.0    # below this speed the car turns less (it cannot turn standing still)
  HIGH_SPEED_STEER = 0.55   # share of the steering left at top speed: steadier when fast
  BOUNCE = -0.25            # speed kept (reversed) after hitting the street's edge
  DEFAULT_BOUND = 1000.0
  HEAD_ON = 0.5             # hitting a car more squarely than this (cosine) bounces off it ...
  SCRAPE_DRAG = 12.0        # ... a glancing touch only slows the car down, by this many m/s^2
  CRASH_SPARKS = 28         # let out where the car hits another squarely ...
  SCRAPE_SPARKS = 2         # ... and every frame it scrapes along one
  SCRAPE_MIN_SPEED = 2.0    # m/s below which a scrape throws no sparks
  SPARK_HEIGHT = 0.5        # meters above the road: bumper height
  # Metal on metal (speed in m/s, life in seconds, size in meters, spread and spin in degrees).
  SPARKS = {
    color: :orange, glow: true, spread: 60, speed: 6.0, life: 0.5, size: 0.22, image: :spark, spin: 360.0
  }.freeze
  POTHOLE_PITCH = 14.0      # degrees of nose dip per meter of drop
  DEGREES_PER_RADIAN = 180.0 / Math::PI
  HINT_SIZE = 18
  HINT_X = 24
  HINT_Y = 24
  HINT_SECONDS = 9.0        # how long the keys are shown after the game starts
  KEYBOARD_HINT = "W/S accelerate and brake   A/D steer   Space hard brake   Mouse looks around".freeze
  GAMEPAD_HINT =
    "Triggers accelerate and brake   Left stick steers   A hard brake   Right stick looks around".freeze

  attr_reader :speed

  def start
    @speed = 0.0
    @steering = 0.0      # -1..1, where the wheel is
    @hole_pitch = 0.0    # degrees, from a pothole
    @face = facing
    @yaw = heading
    @ride_height = transform.position.y
    read_the_street
    gather_parts
  end

  # Where the car's nose points (flat, normalized). Usable before start (the camera may start first).
  def nose = (transform.forward * facing).flat.normalized

  # The gear the engine is in (1 is the first).
  def gear = EngineSound.gear(@speed)

  def update(dt)
    drive(dt) unless dt.zero?  # paused
    draw_hud
  end

  private

  # 1 when the model's nose points along its forward, -1 when it points the other way.
  def facing = prop(:reversed, false) ? -1.0 : 1.0

  # The yaw the car starts at, in degrees. From the forward vector, not the Euler angles: a half
  # turn can read as "flipped, yaw 0".
  def heading
    forward = transform.forward
    Math.atan2(-forward.x, -forward.z) * DEGREES_PER_RADIAN
  end

  def read_the_street
    @min_x = prop(:min_x, -DEFAULT_BOUND)
    @max_x = prop(:max_x, DEFAULT_BOUND)
    @min_z = prop(:min_z, -DEFAULT_BOUND)
    @max_z = prop(:max_z, DEFAULT_BOUND)
  end

  def gather_parts
    @potholes = Potholes.new
    @body = get_component(CarBody)
    @springs = BodySprings.new
    @tire_smoke = TireSmoke.new
    @speedometer = Speedometer.new
    @engine = EngineSound.new
  end

  def drive(dt)
    before = @speed
    throttle = pedals
    accelerate(throttle, dt)
    turning = steer(Input.axis(:horizontal), dt)
    move(dt)
    lean((@speed - before) / dt, turning, dt)
    @engine.rev(@speed, throttle, dt)
  end

  # From -1 (brake, then reverse) to 1 (full throttle): a gamepad's triggers, or the keys. The
  # triggers come first: the stick that steers also moves the :vertical axis a little.
  def pedals
    triggers = Input.gamepad_axis(:right_trigger) - Input.gamepad_axis(:left_trigger)
    triggers.zero? ? Input.axis(:vertical) : triggers
  end

  def accelerate(throttle, dt)
    return hard_brake(dt) if Input.action?(:jump)  # Space, or a gamepad's A
    return slow_down(ROLLING_DRAG, dt) if throttle.zero?
    return slow_down(BRAKING, dt) if (throttle * @speed).negative?  # pressing against the motion brakes first

    speed_up(throttle, dt)
  end

  def hard_brake(dt)
    @tire_smoke.puff(transform.position, flat_nose, @speed, dt)
    slow_down(HARD_BRAKING, dt)
  end

  def slow_down(braking, dt)
    @speed = approach(@speed, 0.0, braking * dt)
  end

  # Strong off the line, running out of breath toward top speed.
  def speed_up(throttle, dt)
    breath = 1.0 - (1.0 - TOP_END_SHARE) * (@speed.abs / MAX_SPEED)
    @speed = (@speed + throttle * ACCELERATION * breath * dt).clamp(-MAX_REVERSE_SPEED, MAX_SPEED)
  end

  # Turns the car; returns how fast it is turning, in degrees per second (left positive).
  def steer(direction, dt)
    @steering += (direction - @steering) * [STEERING_QUICKNESS * dt, 1.0].min
    turning = -@steering * STEER_DEGREES_PER_SECOND * grip * steadiness * rolling
    @yaw += turning * dt
    turning
  end

  # How much of the steering the car's speed leaves it: none standing still ...
  def grip = [@speed.abs / FULL_STEER_SPEED, 1.0].min

  # ... and less again when fast.
  def steadiness = 1.0 - (1.0 - HIGH_SPEED_STEER) * (@speed.abs / MAX_SPEED)

  # Which way the car rolls: 1 forward, -1 in reverse (where it also turns the other way, like a
  # real car).
  def rolling = @speed.negative? ? -1.0 : 1.0

  def move(dt)
    target = transform.position + flat_nose * (@speed * dt)
    on_the_street = inside_the_street(target)
    @speed *= BOUNCE unless on_the_street == target
    clear = inside_the_street(clear_of_cars(on_the_street, dt))
    @body.velocity = flat_nose * @speed if @body
    settle_at(clear)
  end

  # Puts the car there, sunk into the pothole under it if there is one.
  def settle_at(position)
    drop = @potholes.drop_at(position)
    @hole_pitch = drop * POTHOLE_PITCH * rolling
    transform.position = Vector3.new(position.x, @ride_height - drop, position.z)
  end

  # Where the nose points, from the heading alone (the body's lean must not steer the car).
  def flat_nose
    radians = @yaw / DEGREES_PER_RADIAN
    Vector3.new(-Math.sin(radians), 0, -Math.cos(radians)) * @face
  end

  # The body on its springs: it pitches with acceleration and rolls out of turns.
  def lean(acceleration, turning, dt)
    @springs.lean(acceleration, turning, @speed, dt)
    pitch = @springs.pitch + @hole_pitch
    transform.rotation = Vector3.new(pitch * @face, @yaw, @springs.roll * @face)
  end

  def inside_the_street(position)
    Vector3.new(position.x.clamp(@min_x, @max_x), position.y, position.z.clamp(@min_z, @max_z))
  end

  # Moves the car out of any car it would overlap at `position`.
  def clear_of_cars(position, dt)
    return position unless @body

    @body.others.reduce(position) { |clear, other| clear_of(other, clear, dt) }
  end

  # `position`, moved out of one car if the two overlap there.
  def clear_of(other, position, dt)
    push = @body.footprint(position).push_out_of(other.footprint)
    return position unless push

    shove = Vector3.new(push[0], 0, push[1])
    hit(position + shove, shove, dt)
    position + shove
  end

  # Driving into a car squarely bounces off it; touching one sideways scrapes along it.
  def hit(position, shove, dt)
    squareness = -shove.normalized.dot(flat_nose * rolling)
    touched = position - shove.normalized * @body.half_width + Vector3.up * SPARK_HEIGHT
    return crash(touched, shove) if squareness > HEAD_ON

    scrape(touched, shove, dt) if squareness.positive?
  end

  def crash(touched, shove)
    sparks(touched, shove, CRASH_SPARKS)
    @speed *= BOUNCE
  end

  def scrape(touched, shove, dt)
    sparks(touched, shove, SCRAPE_SPARKS) if @speed.abs > SCRAPE_MIN_SPEED
    slow_down(SCRAPE_DRAG, dt)
  end

  # Sparks thrown back from where the cars touch.
  def sparks(from, away, count)
    Particles.burst(from, count: count, direction: away + Vector3.up, **SPARKS)
  end

  def draw_hud
    @speedometer.draw(@speed, MAX_SPEED, gear)
    return unless Time.elapsed < HINT_SECONDS

    UI.text(Input.gamepad? ? GAMEPAD_HINT : KEYBOARD_HINT, x: HINT_X, y: HINT_Y, size: HINT_SIZE)
  end

  def approach(value, goal, step)
    value > goal ? [value - step, goal].max : [value + step, goal].min
  end
end
