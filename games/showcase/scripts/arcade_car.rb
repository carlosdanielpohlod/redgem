# Arcade driving for a car without wheel physics: W/S accelerate and brake (then reverse),
# A/D steer, Space is a hard brake. With a gamepad: the triggers accelerate and brake, the left
# stick steers, A is the hard brake. The body leans into what the car is doing (it squats when it
# accelerates, dives when it brakes, rolls in turns), the engine is heard climbing through its
# gears, and a speedometer is drawn. The car stays inside the street, cannot drive through other
# cars (it needs a CarBody on the same object for that) and drops into potholes: the scene's
# objects tagged "pothole", each with its `radius` and `depth` in meters.
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
  KMH_PER_MS = 3.6
  DEFAULT_BOUND = 1000.0
  POTHOLE_TAG = "pothole"
  HEAD_ON = 0.5             # hitting a car more squarely than this (cosine) bounces off it ...
  SCRAPE_DRAG = 12.0        # ... a glancing touch only slows the car down, by this many m/s^2
  CRASH_SPARKS = 28         # let out where the car hits another squarely ...
  SCRAPE_SPARKS = 2         # ... and every frame it scrapes along one
  SPARK_SPEED = 6.0
  SPARK_LIFE = 0.5
  SPARK_SIZE = 0.22
  SPARK_SPIN = 360.0        # degrees per second
  SMOKE_MIN_SPEED = 8.0     # m/s above which hard braking smokes the tires
  SMOKE_SECONDS = 0.04      # between puffs
  SMOKE_SIZE = 1.3
  SMOKE_LIFE = 1.4
  SMOKE_ALPHA = 70
  SMOKE_COLOR = [120, 120, 124].freeze   # in the dusk's light, white smoke would shine
  SMOKE_HEIGHT = 0.45       # meters above the road
  SMOKE_SPEED = 0.8
  SMOKE_RISE = -0.04        # share of gravity, upside down: it drifts up
  SMOKE_SPIN = 50.0
  REAR_AXLE = 1.3           # meters behind the car's middle
  SPARK_HEIGHT = 0.5        # meters above the road: bumper height
  SCRAPE_MIN_SPEED = 2.0    # m/s below which a scrape throws no sparks
  POTHOLE_DROP = 0.7        # share of a pothole's depth the car body follows (the suspension takes the rest)
  POTHOLE_PITCH = 14.0      # degrees of nose dip per meter of drop
  DEGREES_PER_RADIAN = 180.0 / Math::PI

  # The body on its springs.
  SQUAT_PER_ACCELERATION = 0.16   # degrees of pitch per m/s^2 (nose up accelerating, down braking)
  MAX_BODY_PITCH = 3.2
  ROLL_PER_TURN = 0.11            # degrees of roll per (degree per second of turning x m/s of speed) / 10
  MAX_BODY_ROLL = 4.5
  SPRING_QUICKNESS = 6.0          # how fast the body settles (per second)

  # The engine: top speed of each gear (m/s); the pitch of its sound across a gear.
  GEARS = [9.0, 16.0, 23.0, 30.5].freeze
  IDLE_PITCH = 0.55
  LOW_REV_PITCH = 0.8
  HIGH_REV_PITCH = 1.55
  ENGINE_SOUND = :engine
  IDLE_VOLUME = 0.22
  DRIVING_VOLUME = 0.5
  VOLUME_QUICKNESS = 4.0

  # The speedometer, in the bottom right corner.
  PANEL_WIDTH = 230
  PANEL_HEIGHT = 104
  PANEL_MARGIN = 28
  PANEL_ALPHA = 150
  PANEL_RADIUS = 14
  SPEED_SIZE = 64
  UNIT_SIZE = 20
  GEAR_SIZE = 34
  BAR_HEIGHT = 6
  BAR_INSET = 18
  HINT_SIZE = 18
  HINT_X = 24
  HINT_Y = 24
  HINT_SECONDS = 9.0

  attr_reader :speed

  def start
    @speed = 0.0
    # From the forward vector, not the Euler angles: a half turn can read as "flipped, yaw 0".
    forward = transform.forward
    @yaw = Math.atan2(-forward.x, -forward.z) * DEGREES_PER_RADIAN
    @face = prop(:reversed, false) ? -1.0 : 1.0
    @min_x = prop(:min_x, -DEFAULT_BOUND)
    @max_x = prop(:max_x, DEFAULT_BOUND)
    @min_z = prop(:min_z, -DEFAULT_BOUND)
    @max_z = prop(:max_z, DEFAULT_BOUND)
    @ride_height = transform.position.y
    @steering = 0.0      # -1..1, where the wheel is
    @body_pitch = 0.0    # degrees, from accelerating and braking
    @body_roll = 0.0     # degrees, from turning
    @hole_pitch = 0.0    # degrees, from a pothole
    @volume = IDLE_VOLUME
    @potholes = find_all(POTHOLE_TAG).map do |hole|
      [hole.transform.position.x, hole.transform.position.z, hole.props[:radius].to_f, hole.props[:depth].to_f]
    end
    @body = get_component(CarBody)
    Audio.music(ENGINE_SOUND, loop: true, volume: @volume)
  end

  # Where the car's nose points (flat, normalized). Usable before start (the camera may start first).
  def nose
    sign = prop(:reversed, false) ? -1.0 : 1.0
    (transform.forward * sign).flat.normalized
  end

  # The gear the engine is in (1 is the first).
  def gear = GEARS.index { |top| @speed.abs < top }.to_i + 1

  def update(dt)
    return draw_hud if dt.zero?  # paused

    before = @speed
    throttle = pedals
    accelerate(throttle, dt)
    turning = steer(Input.axis(:horizontal), dt)
    move(dt)
    lean((@speed - before) / dt, turning, dt)
    sound(throttle, dt)
    draw_hud
  end

  private

  # From -1 (brake, then reverse) to 1 (full throttle): a gamepad's triggers, or the keys. The
  # triggers come first: the stick that steers also moves the :vertical axis a little.
  def pedals
    triggers = Input.gamepad_axis(:right_trigger) - Input.gamepad_axis(:left_trigger)
    triggers.zero? ? Input.axis(:vertical) : triggers
  end

  def accelerate(throttle, dt)
    if Input.action?(:jump)  # Space, or a gamepad's A
      tire_smoke(dt)
      @speed = approach(@speed, 0.0, HARD_BRAKING * dt)
    elsif throttle.zero?
      @speed = approach(@speed, 0.0, ROLLING_DRAG * dt)
    elsif throttle * @speed < 0.0
      @speed = approach(@speed, 0.0, BRAKING * dt)  # pressing against the motion brakes first
    else
      # Strong off the line, running out of breath toward top speed.
      breath = 1.0 - (1.0 - TOP_END_SHARE) * (@speed.abs / MAX_SPEED)
      @speed = (@speed + throttle * ACCELERATION * breath * dt).clamp(-MAX_REVERSE_SPEED, MAX_SPEED)
    end
  end

  # Turns the car; returns how fast it is turning, in degrees per second (left positive).
  def steer(direction, dt)
    @steering += (direction - @steering) * [STEERING_QUICKNESS * dt, 1.0].min
    grip = [@speed.abs / FULL_STEER_SPEED, 1.0].min
    steadiness = 1.0 - (1.0 - HIGH_SPEED_STEER) * (@speed.abs / MAX_SPEED)
    sign = @speed.negative? ? -1.0 : 1.0  # reversing turns the other way, like a real car
    turning = -@steering * STEER_DEGREES_PER_SECOND * grip * steadiness * sign
    @yaw += turning * dt
    turning
  end

  def move(dt)
    target = transform.position + flat_nose * (@speed * dt)
    clamped = inside_the_street(target)
    @speed *= BOUNCE if clamped.x != target.x || clamped.z != target.z
    clamped = inside_the_street(clear_of_cars(clamped, dt))
    @body.velocity = flat_nose * @speed if @body
    drop = pothole_drop(clamped)
    @hole_pitch = drop * POTHOLE_PITCH * (@speed.negative? ? -1.0 : 1.0)
    transform.position = Vector3.new(clamped.x, @ride_height - drop, clamped.z)
  end

  # Where the nose points, from the heading alone (the body's lean must not steer the car).
  def flat_nose
    radians = @yaw / DEGREES_PER_RADIAN
    Vector3.new(-Math.sin(radians), 0, -Math.cos(radians)) * @face
  end

  # The body on its springs: it pitches with acceleration and rolls out of turns.
  def lean(acceleration, turning, dt)
    settle = [SPRING_QUICKNESS * dt, 1.0].min
    pitch = (-acceleration * SQUAT_PER_ACCELERATION).clamp(-MAX_BODY_PITCH, MAX_BODY_PITCH)
    roll = (turning * @speed * ROLL_PER_TURN / 10.0).clamp(-MAX_BODY_ROLL, MAX_BODY_ROLL)
    @body_pitch += (pitch - @body_pitch) * settle
    @body_roll += (roll - @body_roll) * settle
    transform.rotation = Vector3.new((@body_pitch + @hole_pitch) * @face, @yaw, @body_roll * @face)
  end

  # The engine's note climbs through each gear and drops when the next one comes in.
  def sound(throttle, dt)
    bottom = gear == 1 ? 0.0 : GEARS[gear - 2]
    top = GEARS[[gear - 1, GEARS.size - 1].min]
    revs = ((@speed.abs - bottom) / (top - bottom)).clamp(0.0, 1.0)
    moving = @speed.abs > 0.5
    Audio.music_pitch = moving ? LOW_REV_PITCH + (HIGH_REV_PITCH - LOW_REV_PITCH) * revs : IDLE_PITCH
    loudness = throttle.zero? ? IDLE_VOLUME : DRIVING_VOLUME
    @volume += (loudness - @volume) * [VOLUME_QUICKNESS * dt, 1.0].min
    Audio.music_volume = @volume
  end

  def inside_the_street(position)
    Vector3.new(position.x.clamp(@min_x, @max_x), position.y, position.z.clamp(@min_z, @max_z))
  end

  # Moves the car out of any car it would overlap at `position`. Driving into one squarely bounces
  # off it; touching one sideways scrapes along it.
  def clear_of_cars(position, dt)
    return position unless @body

    @body.others.each do |other|
      push = @body.footprint(position).push_out_of(other.footprint)
      next unless push

      shove = Vector3.new(push[0], 0, push[1])
      position += shove
      travel = flat_nose * (@speed.negative? ? -1.0 : 1.0)
      squareness = -shove.normalized.dot(travel)
      touched = position - shove.normalized * @body.half_width + Vector3.up * SPARK_HEIGHT
      if squareness > HEAD_ON
        sparks(touched, shove, CRASH_SPARKS)
        @speed *= BOUNCE
      elsif squareness.positive?
        sparks(touched, shove, SCRAPE_SPARKS) if @speed.abs > SCRAPE_MIN_SPEED
        @speed = approach(@speed, 0.0, SCRAPE_DRAG * dt)
      end
    end
    position
  end

  # Metal on metal: sparks thrown back from where the cars touch.
  def sparks(at, away, count)
    Particles.burst(at, count: count, color: :orange, glow: true, direction: away + Vector3.up, spread: 60,
                    speed: SPARK_SPEED, life: SPARK_LIFE, size: SPARK_SIZE, image: :spark, spin: SPARK_SPIN)
  end

  # Locked wheels at speed: puffs of smoke left behind the rear axle.
  def tire_smoke(dt)
    @smoke_wait = (@smoke_wait || 0.0) - dt
    return unless @smoke_wait <= 0.0 && @speed.abs > SMOKE_MIN_SPEED

    @smoke_wait = SMOKE_SECONDS
    Particles.burst(transform.position - flat_nose * REAR_AXLE + Vector3.up * SMOKE_HEIGHT, count: 1, image: :smoke,
                    color: SMOKE_COLOR, alpha: SMOKE_ALPHA,
                    speed: SMOKE_SPEED, life: SMOKE_LIFE, size: SMOKE_SIZE, gravity: SMOKE_RISE, spin: SMOKE_SPIN)
  end

  # How far the car sinks at a position: the deepest pothole under it, eased toward its edge.
  def pothole_drop(position)
    @potholes.map do |x, z, radius, depth|
      distance = Math.sqrt((position.x - x)**2 + (position.z - z)**2)
      distance < radius ? depth * POTHOLE_DROP * (1.0 - distance / radius) : 0.0
    end.max || 0.0
  end

  def draw_hud
    x = UI.width - PANEL_WIDTH - PANEL_MARGIN
    y = UI.height - PANEL_HEIGHT - PANEL_MARGIN
    UI.rect(x: x, y: y, width: PANEL_WIDTH, height: PANEL_HEIGHT, color: :black, alpha: PANEL_ALPHA, radius: PANEL_RADIUS)
    speed = (@speed.abs * KMH_PER_MS).round.to_s
    UI.text(speed, x: x + PANEL_WIDTH - BAR_INSET - 62, y: y + 12, size: SPEED_SIZE, align: :right)
    UI.text("km/h", x: x + PANEL_WIDTH - BAR_INSET, y: y + 50, size: UNIT_SIZE, color: :gray, align: :right)
    UI.text(@speed.negative? ? "R" : gear.to_s, x: x + BAR_INSET, y: y + 26, size: GEAR_SIZE, color: :ruby)
    # How far up the speed range the car is.
    track = PANEL_WIDTH - BAR_INSET * 2
    UI.rect(x: x + BAR_INSET, y: y + PANEL_HEIGHT - BAR_INSET, width: track, height: BAR_HEIGHT, color: :gray, alpha: 110, radius: 3)
    filled = (track * (@speed.abs / MAX_SPEED)).to_i
    UI.rect(x: x + BAR_INSET, y: y + PANEL_HEIGHT - BAR_INSET, width: filled, height: BAR_HEIGHT, color: :ruby, radius: 3) if filled.positive?
    return unless Time.elapsed < HINT_SECONDS

    hint = if Input.gamepad?
             "Triggers accelerate and brake   Left stick steers   A hard brake   Right stick looks around"
           else
             "W/S accelerate and brake   A/D steer   Space hard brake   Mouse looks around"
           end
    UI.text(hint, x: HINT_X, y: HINT_Y, size: HINT_SIZE)
  end

  def approach(value, goal, step)
    value > goal ? [value - step, goal].max : [value + step, goal].min
  end
end
