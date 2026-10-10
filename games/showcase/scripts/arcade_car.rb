# Arcade driving for a car without wheel physics: W/S accelerate and brake (then reverse),
# A/D steer, Space is a hard brake. The car stays inside the street, cannot drive through other
# cars (it needs a CarBody on the same object for that) and drops into potholes: the scene's
# objects tagged "pothole", each with its `radius` and `depth` in meters.
# Settings: reversed (the model's nose points along -forward), min_x, max_x, min_z, max_z (street
# bounds in meters).
class ArcadeCar < Behaviour
  MAX_SPEED = 22.0          # m/s, ~80 km/h
  MAX_REVERSE_SPEED = 6.0
  ACCELERATION = 7.0        # m/s^2
  BRAKING = 16.0
  HARD_BRAKING = 26.0
  ROLLING_DRAG = 2.0
  STEER_DEGREES_PER_SECOND = 75.0
  FULL_STEER_SPEED = 6.0    # below this speed the car turns less (it cannot turn standing still)
  BOUNCE = -0.25            # speed kept (reversed) after hitting the street's edge
  KMH_PER_MS = 3.6
  HUD_X = 24
  HUD_Y = 24
  HUD_SIZE = 32
  HINT_Y = 64
  HINT_SIZE = 20
  DEFAULT_BOUND = 1000.0
  POTHOLE_TAG = "pothole"
  HEAD_ON = 0.5             # hitting a car more squarely than this (cosine) bounces off it ...
  SCRAPE_DRAG = 12.0        # ... a glancing touch only slows the car down, by this many m/s^2
  POTHOLE_DROP = 0.7        # share of a pothole's depth the car body follows (the suspension takes the rest)
  POTHOLE_PITCH = 14.0      # degrees of nose dip per meter of drop
  DEGREES_PER_RADIAN = 180.0 / Math::PI

  attr_reader :speed

  def start
    @speed = 0.0
    # From the forward vector, not the Euler angles: a half turn can read as "flipped, yaw 0".
    forward = transform.forward
    @yaw = Math.atan2(-forward.x, -forward.z) * DEGREES_PER_RADIAN
    @min_x = prop(:min_x, -DEFAULT_BOUND)
    @max_x = prop(:max_x, DEFAULT_BOUND)
    @min_z = prop(:min_z, -DEFAULT_BOUND)
    @max_z = prop(:max_z, DEFAULT_BOUND)
    @ride_height = transform.position.y
    @pitch = 0.0
    @potholes = find_all(POTHOLE_TAG).map do |hole|
      [hole.transform.position.x, hole.transform.position.z, hole.props[:radius].to_f, hole.props[:depth].to_f]
    end
    @body = get_component(CarBody)
  end

  # Where the car's nose points (flat, normalized). Usable before start (the camera may start first).
  def nose
    sign = prop(:reversed, false) ? -1.0 : 1.0
    (transform.forward * sign).flat.normalized
  end

  def update(dt)
    accelerate(Input.axis(:vertical), dt)
    steer(Input.axis(:horizontal), dt)
    move(dt)
    draw_hud
  end

  private

  def accelerate(throttle, dt)
    if Input.down?(:space)
      @speed = approach(@speed, 0.0, HARD_BRAKING * dt)
    elsif throttle.zero?
      @speed = approach(@speed, 0.0, ROLLING_DRAG * dt)
    elsif throttle * @speed < 0.0
      @speed = approach(@speed, 0.0, BRAKING * dt)  # pressing against the motion brakes first
    else
      @speed = (@speed + throttle * ACCELERATION * dt).clamp(-MAX_REVERSE_SPEED, MAX_SPEED)
    end
  end

  def steer(direction, dt)
    grip = [@speed.abs / FULL_STEER_SPEED, 1.0].min
    sign = @speed.negative? ? -1.0 : 1.0  # reversing turns the other way, like a real car
    @yaw -= direction * STEER_DEGREES_PER_SECOND * grip * sign * dt
    transform.rotation = Vector3.new(@pitch, @yaw, 0)
  end

  def move(dt)
    target = transform.position + nose * (@speed * dt)
    clamped = inside_the_street(target)
    @speed *= BOUNCE if clamped.x != target.x || clamped.z != target.z
    clamped = inside_the_street(clear_of_cars(clamped, dt))
    @body.velocity = nose * @speed if @body
    drop = pothole_drop(clamped)
    @pitch = drop * POTHOLE_PITCH * (@speed.negative? ? -1.0 : 1.0)
    transform.position = Vector3.new(clamped.x, @ride_height - drop, clamped.z)
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
      travel = nose * (@speed.negative? ? -1.0 : 1.0)
      squareness = -shove.normalized.dot(travel)
      if squareness > HEAD_ON
        @speed *= BOUNCE
      elsif squareness.positive?
        @speed = approach(@speed, 0.0, SCRAPE_DRAG * dt)
      end
    end
    position
  end

  # How far the car sinks at a position: the deepest pothole under it, eased toward its edge.
  def pothole_drop(position)
    @potholes.map do |x, z, radius, depth|
      distance = Math.sqrt((position.x - x)**2 + (position.z - z)**2)
      distance < radius ? depth * POTHOLE_DROP * (1.0 - distance / radius) : 0.0
    end.max || 0.0
  end

  def draw_hud
    UI.text("#{(@speed.abs * KMH_PER_MS).round} km/h", x: HUD_X, y: HUD_Y, size: HUD_SIZE)
    UI.text("W/S accelerate / brake   A/D steer   Space hard brake", x: HUD_X, y: HINT_Y, size: HINT_SIZE)
  end

  def approach(value, goal, step)
    value > goal ? [value - step, goal].max : [value + step, goal].min
  end
end
