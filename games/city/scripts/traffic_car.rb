# A car driving its lane in a loop: it stops at red lights, keeps its distance from the car ahead
# and spins its wheels. Blender props: axis ("avenue"/"street"), dir_x, dir_y (Blender axes),
# speed (m/s), half_length (m from the crossroad to where the road ends).
class TrafficCar < Behaviour
  DEFAULT_SPEED = 9.0
  DEFAULT_HALF_LENGTH = 60.0   # meters, when the builder did not say how long the road is
  DEFAULT_AXIS = "avenue"
  FULL_ROAD = 2.0              # a car leaving one end comes back at the other: two half lengths
  ACCELERATION = 6.0       # m/s^2
  BRAKING = 14.0           # m/s^2
  # Distance from the crossroad center to the stop line (the crossroad tile is 8 m wide).
  STOP_LINE = 7.5
  # Cars closer than this to the stop line when the light turns red keep going.
  COMMIT_DISTANCE = 1.5
  SAFE_GAP = 9.0           # m to the car ahead
  STOP_MARGIN = 1.0        # m before the stop line
  LANE_WIDTH = 2.0         # m: cars within this sideways distance share a lane
  WHEEL_RADIUS = 0.53      # m, Kenney car wheels at the scene's car scale
  RADIANS_TO_DEGREES = 180.0 / Math::PI
  CROSSROAD = Vector3.zero

  # Kenney cars name their road wheels wheel-front-left ... wheel-back-right; "wheel-back" alone is
  # a spare tire mounted on the body, which must not spin.
  ROLLING_WHEEL_PREFIXES = %w[wheel-front- wheel-back-].freeze

  attr_reader :speed

  def self.rolling_wheel?(object) = ROLLING_WHEEL_PREFIXES.any? { |prefix| object.name.start_with?(prefix) }

  def start
    @direction = Vector3.blender(prop(:dir_x, 0.0), prop(:dir_y, 1.0), 0.0).normalized
    @cruise = prop(:speed, DEFAULT_SPEED)
    @half_length = prop(:half_length, DEFAULT_HALF_LENGTH)
    @axis = prop(:axis, DEFAULT_AXIS)
    @speed = @cruise
    @wheels = game_object.children.select { |child| TrafficCar.rolling_wheel?(child) }
  end

  def update(dt)
    target = must_stop? ? 0.0 : @cruise
    @speed = approach(@speed, target, (target < @speed ? BRAKING : ACCELERATION) * dt)
    transform.translate(@direction * (@speed * dt))
    wrap_around
    spin_wheels(dt)
  end

  # Signed distance along the lane from the crossroad (negative = still approaching it).
  def along = (transform.position - CROSSROAD).dot(@direction)

  private

  def must_stop?
    red_light_ahead? || car_ahead?
  end

  def red_light_ahead?
    controller = TrafficController.instance
    return false if controller.nil? || controller.green?(@axis)

    distance_to_line = -STOP_LINE - along
    distance_to_line > -COMMIT_DISTANCE && distance_to_line < braking_distance + STOP_MARGIN
  end

  def car_ahead?
    Scene.find_all(:car).any? do |other|
      next false if other == game_object

      offset = other.transform.position - transform.position
      ahead = offset.dot(@direction)
      sideways = (offset - @direction * ahead).length
      ahead > 0 && ahead < SAFE_GAP + braking_distance && sideways < LANE_WIDTH
    end
  end

  def braking_distance = (@speed * @speed) / (2.0 * BRAKING)

  # Leaving one end of the road brings the car back at the other end.
  def wrap_around
    return if along < @half_length

    transform.position = transform.position - @direction * (@half_length * FULL_ROAD)
  end

  def spin_wheels(dt)
    degrees = (@speed * dt / WHEEL_RADIUS) * RADIANS_TO_DEGREES
    @wheels.each { |wheel| wheel.transform.rotate(Vector3.new(degrees, 0, 0)) }
  end

  def approach(value, target, step)
    value < target ? [value + step, target].min : [value - step, target].max
  end
end
