# A car that drives along its lane (the street runs along Z) and comes back around at the end of
# the street. It keeps its distance from the car in front, stops behind what blocks its lane, and
# goes around a standing car when there is room: on its own half of the road, or on the other
# half while nobody is coming.
# Needs a CarBody on the same object to see the other cars; without one it just drives.
# Settings: speed (m/s), reversed (the model's nose points along -forward), min_x, max_x (how far
# across the road its center may go), min_z, max_z (ends of the street), all in meters.
class TrafficCar < Behaviour
  DEFAULT_SPEED = 9.0
  DEFAULT_BOUND = 1000.0
  ACCELERATION = 3.0        # m/s^2
  BRAKING = 9.0             # the hardest it brakes
  PLANNED_BRAKING = 5.0     # what it counts on when choosing its speed: it brakes early, not hard
  LOOK_AHEAD = 45.0         # meters: what it slows down for
  PASS_AHEAD = 40.0         # meters: how close a standing car is when it starts moving over
  STOP_GAP = 2.5            # meters left to the car in front when stopped behind it
  SIDE_GAP = 0.25           # meters left to a car it passes
  CENTER_GAP = 0.1          # meters from the middle of the road within which it is on its own half
  ONCOMING_CLEAR = 70.0     # meters of empty road it wants ahead before using the other half
  COMMITTED_GAP = 4.0       # this close to the car it is passing, it finishes passing
  NEARLY = 0.001            # meters: slack when comparing distances that should be equal
  LANE_MARGIN = 0.1         # cars this close to its path (sideways, m) are in its way
  STANDING_SPEED = 0.5      # a car slower than this is passed, not followed
  PASSED_BY = 1.5           # meters past a standing car before moving back
  # Changing lanes: the car turns in and out gently, so its corners do not swing into the cars
  # beside it.
  SIDEWAYS_SHARE = 0.12     # the most it goes sideways, as a share of its forward speed
  STEERING_RATE = 0.3       # how fast that share builds up and fades, per second
  STRAIGHTEN_OVER = 4.0     # meters before the line it is heading for where it starts to straighten
  ON_THE_LINE = 0.005       # meters: close enough to be there
  MIN_TURNING_SPEED = 0.2   # below this the car keeps pointing where it did
  DEGREES_PER_RADIAN = 180.0 / Math::PI

  # Another car, as seen from this one: `ahead` is how far its center is down the lane (negative:
  # behind), `gap` the room between the bumpers, `speed` how fast it goes this car's way.
  Neighbour = Struct.new(:footprint, :ahead, :gap, :speed)

  attr_reader :speed

  def start
    @cruise = prop(:speed, DEFAULT_SPEED)
    @speed = @cruise
    @face = prop(:reversed, false) ? -1.0 : 1.0
    @heading = (transform.forward * @face).z.negative? ? -1.0 : 1.0  # which way along Z it drives
    @lane_x = transform.position.x
    @inward = @lane_x.positive? ? -1.0 : 1.0                         # toward the middle of the road
    @min_x = prop(:min_x, -DEFAULT_BOUND)
    @max_x = prop(:max_x, DEFAULT_BOUND)
    @target_x = @lane_x
    @steering = 0.0
    @min_z = prop(:min_z, -DEFAULT_BOUND)
    @max_z = prop(:max_z, DEFAULT_BOUND)
    @body = get_component(CarBody)
  end

  def update(dt)
    position = transform.position
    neighbours = neighbours_of(position)
    limit = [@cruise, allowed_speed(position, neighbours)].min
    @speed = approach(@speed, limit, (limit < @speed ? BRAKING : ACCELERATION) * dt)
    sideways = steer_toward(passing_x(neighbours), position.x, dt)
    x = position.x + sideways * dt
    transform.position = Vector3.new(x, position.y, around_the_street(position.z + @heading * @speed * dt))
    point_along(sideways)
    @body.velocity = Vector3.new(sideways, 0, @heading * @speed) if @body
  end

  private

  def street_length = @max_z - @min_z

  # Past one end of the street: reappear at the other.
  def around_the_street(z)
    z += street_length if z < @min_z
    z -= street_length if z > @max_z
    z
  end

  # The other cars near this one. The lane is a loop (cars come back around), so a car close to the
  # far end of the street is right ahead of one about to leave at this end.
  def neighbours_of(position)
    return [] unless @body

    @body.others.map do |other|
      footprint = other.footprint
      ahead = ((footprint.z - position.z) * @heading) % street_length
      ahead -= street_length if ahead > street_length * 0.5
      gap = ahead.abs - @body.half_length - footprint.half_extent_z
      Neighbour.new(footprint, ahead, gap, other.velocity.z * @heading)
    end
  end

  # How far apart (sideways) the centers must be for this car to clear another.
  def room_beside(neighbour) = @body.half_width + neighbour.footprint.half_extent_x

  # The fastest it may go and still stop behind whatever is in its way.
  def allowed_speed(position, neighbours)
    in_the_way = neighbours.select do |neighbour|
      neighbour.ahead.positive? && neighbour.gap < LOOK_AHEAD &&
        (neighbour.footprint.x - position.x).abs < room_beside(neighbour) + LANE_MARGIN
    end
    in_the_way.map do |neighbour|
      room = [neighbour.gap - STOP_GAP, 0.0].max
      [neighbour.speed, 0.0].max + Math.sqrt(2.0 * PLANNED_BRAKING * room)
    end.min || @cruise
  end

  # Where across the road it wants to be: its lane, or the nearest line that clears the standing
  # cars ahead of and beside it. When there is none it stays in its lane (and stops behind them).
  def passing_x(neighbours)
    return @lane_x unless @body

    standing = neighbours.select do |neighbour|
      neighbour.speed.abs < STANDING_SPEED && neighbour.gap < PASS_AHEAD &&
        (neighbour.ahead.positive? || neighbour.gap < PASSED_BY)
    end
    return @target_x if committed?(standing)

    lines = [@lane_x] + standing.map { |neighbour| neighbour.footprint.x + @inward * passing_room(neighbour) }
    @target_x = lines.sort_by { |x| (x - @lane_x).abs }.find { |x| clear_line?(x, standing, neighbours) } || @lane_x
  end

  def passing_room(neighbour) = room_beside(neighbour) + SIDE_GAP

  def beside?(neighbour, x) = (neighbour.footprint.x - x).abs < passing_room(neighbour) - NEARLY

  # Already out of its lane and next to what it is passing: no changing its mind now.
  def committed?(standing)
    @target_x != @lane_x && standing.any? { |neighbour| neighbour.gap < COMMITTED_GAP && beside?(neighbour, @lane_x) }
  end

  def clear_line?(x, standing, neighbours)
    return false if x < @min_x || x > @max_x || standing.any? { |neighbour| beside?(neighbour, x) }

    own_half = x * -@inward >= @body.half_width + CENTER_GAP - NEARLY
    own_half || neighbours.none? do |neighbour|
      neighbour.speed < -STANDING_SPEED && neighbour.ahead > -PASSED_BY && neighbour.gap < ONCOMING_CLEAR &&
        beside?(neighbour, x)
    end
  end

  # The sideways speed (m/s) that takes the car toward the line at `goal`, easing in and out.
  def steer_toward(goal, x, dt)
    off = goal - x
    wanted = off.abs < ON_THE_LINE ? 0.0 : [SIDEWAYS_SHARE, off.abs / STRAIGHTEN_OVER].min * (off.negative? ? -1.0 : 1.0)
    @steering = approach(@steering, wanted, STEERING_RATE * dt)
    sideways = @steering * @speed
    reach = off.abs / dt
    dt.positive? && sideways.abs > reach ? reach * (off.negative? ? -1.0 : 1.0) : sideways  # never past the line
  end

  # Turns the nose the way the car is really going (a little sideways while changing lanes).
  def point_along(sideways)
    return if @speed < MIN_TURNING_SPEED

    forward = Vector3.new(sideways, 0, @heading * @speed) * @face
    transform.rotation = Vector3.new(0, Math.atan2(-forward.x, -forward.z) * DEGREES_PER_RADIAN, 0)
  end

  def approach(value, goal, step)
    value > goal ? [value - step, goal].max : [value + step, goal].min
  end
end
