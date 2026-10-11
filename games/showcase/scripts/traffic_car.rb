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
  HALF = 0.5

  # Another car, as seen from this one: `ahead` is how far its center is down the lane (negative:
  # behind), `gap` the room between the bumpers, `speed` how fast it goes this car's way.
  Neighbour = Struct.new(:footprint, :ahead, :gap, :speed)

  attr_reader :speed

  def start
    @cruise = prop(:speed, DEFAULT_SPEED)
    @speed = @cruise
    @steering = 0.0
    @body = get_component(CarBody)
    find_the_lane
    read_the_street
  end

  def update(dt)
    position = transform.position
    neighbours = neighbours_of(position)
    keep_pace(allowed_speed(position, neighbours), dt)
    advance(position, steer_toward(passing_x(neighbours), position.x, dt), dt)
  end

  private

  def find_the_lane
    @face = prop(:reversed, false) ? -1.0 : 1.0
    @heading = (transform.forward * @face).z.negative? ? -1.0 : 1.0  # which way along Z it drives
    @lane_x = transform.position.x
    @inward = @lane_x.positive? ? -1.0 : 1.0                         # toward the middle of the road
    @target_x = @lane_x
  end

  def read_the_street
    @min_x = prop(:min_x, -DEFAULT_BOUND)
    @max_x = prop(:max_x, DEFAULT_BOUND)
    @min_z = prop(:min_z, -DEFAULT_BOUND)
    @max_z = prop(:max_z, DEFAULT_BOUND)
  end

  # Speeds up or brakes toward its cruising speed, or what the cars ahead allow if that is less.
  def keep_pace(allowed, dt)
    limit = [@cruise, allowed].min
    @speed = approach(@speed, limit, (limit < @speed ? BRAKING : ACCELERATION) * dt)
  end

  # Moves the car for this frame: down its lane, and `sideways` (m/s) across the road.
  def advance(position, sideways, dt)
    z = around_the_street(position.z + @heading * @speed * dt)
    transform.position = Vector3.new(position.x + sideways * dt, position.y, z)
    point_along(sideways)
    @body.velocity = Vector3.new(sideways, 0, @heading * @speed) if @body
  end

  def street_length = @max_z - @min_z

  # Past one end of the street: reappear at the other.
  def around_the_street(z)
    z += street_length if z < @min_z
    z -= street_length if z > @max_z
    z
  end

  def neighbours_of(position)
    return [] unless @body

    @body.others.map { |other| neighbour(other, position.z) }
  end

  # Another car as seen from this one, which is at `z` down the street.
  def neighbour(other, z)
    footprint = other.footprint
    ahead = distance_ahead(footprint.z - z)
    gap = ahead.abs - @body.half_length - footprint.half_extent_z
    Neighbour.new(footprint, ahead, gap, other.velocity.z * @heading)
  end

  # How far down the lane something `offset_z` away along Z is. The lane is a loop (cars come back
  # around), so a car close to the far end of the street is right ahead of one about to leave at
  # this end.
  def distance_ahead(offset_z)
    ahead = (offset_z * @heading) % street_length
    ahead > street_length * HALF ? ahead - street_length : ahead
  end

  # How far apart (sideways) the centers must be for this car to clear another.
  def room_beside(neighbour) = @body.half_width + neighbour.footprint.half_extent_x

  # The fastest it may go and still stop behind whatever is in its way.
  def allowed_speed(position, neighbours)
    in_the_way = neighbours.select { |neighbour| in_the_way?(neighbour, position.x) }
    in_the_way.map { |neighbour| speed_behind(neighbour) }.min || @cruise
  end

  def in_the_way?(neighbour, x)
    neighbour.ahead.positive? && neighbour.gap < LOOK_AHEAD &&
      (neighbour.footprint.x - x).abs < room_beside(neighbour) + LANE_MARGIN
  end

  # The speed from which it still stops in the room left behind a car: v^2 = 2 a d, on top of
  # what that car does itself.
  def speed_behind(neighbour)
    room = [neighbour.gap - STOP_GAP, 0.0].max
    [neighbour.speed, 0.0].max + Math.sqrt(2.0 * PLANNED_BRAKING * room)
  end

  # Where across the road it wants to be: its lane, or the nearest line that clears the standing
  # cars ahead of and beside it. When there is none it stays in its lane (and stops behind them).
  def passing_x(neighbours)
    return @lane_x unless @body

    standing = cars_to_pass(neighbours)
    return @target_x if committed?(standing)

    @target_x = lines_past(standing).find { |x| clear_line?(x, standing, neighbours) } || @lane_x
  end

  def cars_to_pass(neighbours) = neighbours.select { |neighbour| to_pass?(neighbour) }

  # A standing car ahead, or one it is still beside.
  def to_pass?(neighbour)
    neighbour.speed.abs < STANDING_SPEED && neighbour.gap < PASS_AHEAD &&
      (neighbour.ahead.positive? || neighbour.gap < PASSED_BY)
  end

  # The lines it could drive along to get past them, the nearest to its lane first.
  def lines_past(standing)
    lines = [@lane_x] + standing.map { |neighbour| neighbour.footprint.x + @inward * passing_room(neighbour) }
    lines.sort_by { |x| (x - @lane_x).abs }
  end

  def passing_room(neighbour) = room_beside(neighbour) + SIDE_GAP

  def beside?(neighbour, x) = (neighbour.footprint.x - x).abs < passing_room(neighbour) - NEARLY

  # Already out of its lane and next to what it is passing: no changing its mind now.
  def committed?(standing)
    @target_x != @lane_x && standing.any? { |neighbour| neighbour.gap < COMMITTED_GAP && beside?(neighbour, @lane_x) }
  end

  # Whether it can drive along the line at `x`: on the road, past the standing cars, and either on
  # its own half of the road or with nobody coming on the other.
  def clear_line?(x, standing, neighbours)
    return false unless x.between?(@min_x, @max_x) && free_of?(standing, x)

    own_half?(x) || free_of?(neighbours.select { |neighbour| oncoming?(neighbour) }, x)
  end

  # Whether none of those cars is in the way of driving along the line at `x`.
  def free_of?(cars, x) = cars.none? { |neighbour| beside?(neighbour, x) }

  def own_half?(x) = x * -@inward >= @body.half_width + CENTER_GAP - NEARLY

  def oncoming?(neighbour)
    neighbour.speed < -STANDING_SPEED && neighbour.ahead > -PASSED_BY && neighbour.gap < ONCOMING_CLEAR
  end

  # The sideways speed (m/s) that takes the car toward the line at `goal`, easing in and out.
  def steer_toward(goal, x, dt)
    off = goal - x
    @steering = approach(@steering, wanted_steering(off), STEERING_RATE * dt)
    sideways = @steering * @speed
    reach = off.abs / dt
    dt.positive? && sideways.abs > reach ? reach * side_of(off) : sideways  # never past the line
  end

  # The share of its speed it wants to go sideways with, `off` meters from the line it heads for.
  def wanted_steering(off)
    return 0.0 if off.abs < ON_THE_LINE

    [SIDEWAYS_SHARE, off.abs / STRAIGHTEN_OVER].min * side_of(off)
  end

  def side_of(offset) = offset.negative? ? -1.0 : 1.0

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
