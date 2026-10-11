# Leaves drifting down the street. Copies of leaf templates fall around the camera, carried by a
# gusty wind and tumbling; a leaf that lands rests for a moment, then starts again from above, so
# there are always leaves in the air wherever the player goes.
# Settings: templates (names of the leaf prefabs to copy), count, radius (m around the camera),
# ceiling (m), rest (seconds a landed leaf lies there), wind_x, wind_z (m/s), min_x, max_x (m, how
# wide the street is), sidewalk_from (m from the middle where the ground is higher),
# sidewalk_height (m).
class FallingLeaves < Behaviour
  DEFAULT_COUNT = 80
  DEFAULT_RADIUS = 26.0
  DEFAULT_CEILING = 9.0
  DEFAULT_WIND = 1.4
  DEFAULT_HALF_WIDTH = 9.0
  SEED = 2024                       # the same swirl every run
  MIN_START_HEIGHT = 0.35           # of the ceiling: new leaves start between this and the top
  BEHIND_SHARE = 0.35               # how far behind the camera leaves may start, as a share of the radius
  MIN_FALL = 0.45                   # m/s
  MAX_FALL = 1.1
  FLUTTER = 0.9                     # m/s of side-to-side swing
  FLUTTER_SPEED = 2.6               # swings per second (radians)
  MAX_SPIN = 260.0                  # degrees per second around each axis
  GUST = 0.6                        # the wind varies by this share of its strength
  GUST_SPEED = 0.7
  DEFAULT_REST = 2.5                # seconds a landed leaf stays before starting over
  GROUND_CLEARANCE = 0.012          # m: resting leaves lie just above the surface
  FULL_TURN = 360.0
  TOO_FAR = 1.4                     # leaves further ahead than this many radii start over
  BEHIND_MARGIN = 1.5               # ... and so do those this many times further behind than new ones start
  MIN_SIDE_SWING = 0.3              # share of the flutter a leaf keeps across a wind that blows along the street

  # One leaf: its object and how it moves.
  class Leaf
    attr_accessor :fall, :phase, :spin, :rest
    attr_reader :position

    def initialize(object)
      @object = object
      @rest = 0.0
    end

    def position=(position)
      @position = position
      @object.transform.position = position
    end

    def rotation=(rotation)
      @object.transform.rotation = rotation
    end

    # Turns the way it spins, for dt seconds.
    def tumble(dt) = @object.transform.rotate(@spin * dt)
  end

  def start
    @random = Random.new(SEED)
    read_the_weather
    read_the_street
    @leaves = spawn_leaves(prop(:templates, []), view_center)
  end

  def update(dt)
    center = view_center
    gust = 1.0 + GUST * Math.sin(Time.elapsed * GUST_SPEED)
    @leaves.each { |leaf| leaf.rest.positive? ? rest(leaf, dt, center) : fall(leaf, dt, center, gust) }
  end

  private

  def read_the_weather
    @radius = prop(:radius, DEFAULT_RADIUS)
    @ceiling = prop(:ceiling, DEFAULT_CEILING)
    @rest = prop(:rest, DEFAULT_REST)
    @wind = Vector3.new(prop(:wind_x, DEFAULT_WIND), 0, prop(:wind_z, 0.0))
  end

  def read_the_street
    @min_x = prop(:min_x, -DEFAULT_HALF_WIDTH)
    @max_x = prop(:max_x, DEFAULT_HALF_WIDTH)
    @sidewalk_from = prop(:sidewalk_from, DEFAULT_HALF_WIDTH)
    @sidewalk_height = prop(:sidewalk_height, 0.0)
  end

  # Copies of the templates, by turns, already spread out in the air around `center`.
  def spawn_leaves(templates, center)
    return [] if templates.empty?

    prop(:count, DEFAULT_COUNT).to_i.times.filter_map { |index| new_leaf(templates[index % templates.size], center) }
  end

  def new_leaf(template, center)
    object = Scene.spawn(template, position: center)
    return unless object

    leaf = Leaf.new(object)
    launch(leaf, center, any_height)
    leaf
  end

  def view_center
    Camera.main ? Camera.main.transform.position : transform.position
  end

  def between(low, high) = low + (high - low) * @random.rand

  def any_height = between(0.0, @ceiling)

  def any_angle = between(0.0, FULL_TURN)

  def any_spin = between(-MAX_SPIN, MAX_SPIN)

  def ground_at(x) = x.abs >= @sidewalk_from ? @sidewalk_height : 0.0

  # Puts a leaf somewhere above the street near the camera (mostly ahead of it: -Z) and gives it
  # its own way of falling.
  def launch(leaf, center, height)
    leaf.position = launch_position(center, height)
    leaf.fall = between(MIN_FALL, MAX_FALL)
    leaf.phase = any_angle
    leaf.spin = Vector3.new(any_spin, any_spin, any_spin)
    leaf.rest = 0.0
    leaf.rotation = Vector3.new(any_angle, any_angle, 0)
  end

  def launch_position(center, height)
    x = between(@min_x, @max_x)
    z = center.z + between(-@radius, @radius * BEHIND_SHARE)
    Vector3.new(x, [height, ground_at(x) + GROUND_CLEARANCE].max, z)
  end

  def fall(leaf, dt, center, gust)
    position = leaf.position + drift(leaf, gust) * dt - Vector3.up * (leaf.fall * dt)
    position = land(leaf, position) if position.y <= resting_height(position)
    # A leaf the camera left behind comes back at any height: driving fast still has leaves at eye level.
    return launch(leaf, center, any_height) if lost?(position, center)

    leaf.position = position
    leaf.tumble(dt) unless leaf.rest.positive?
  end

  # How fast the air carries a leaf (m/s): the wind in its gusts, and the leaf's own swinging.
  def drift(leaf, gust)
    swing = Math.sin(Time.elapsed * FLUTTER_SPEED + leaf.phase) * FLUTTER
    @wind * gust + Vector3.new(swing * @wind.z.abs.clamp(MIN_SIDE_SWING, 1.0), 0, swing)
  end

  # Where a leaf lies once it is down at `position`: just above the road or the sidewalk.
  def resting_height(position) = ground_at(position.x) + GROUND_CLEARANCE

  # The leaf touched the ground: it lies flat there for a while. Returns where.
  def land(leaf, position)
    leaf.rest = @rest
    leaf.rotation = Vector3.new(0, any_angle, 0)
    position.with(y: resting_height(position))
  end

  def rest(leaf, dt, center)
    leaf.rest -= dt
    return launch(leaf, center, any_height) if lost?(leaf.position, center)

    launch(leaf, center, between(@ceiling * MIN_START_HEIGHT, @ceiling)) unless leaf.rest.positive?
  end

  # Blown off the street, far ahead, or left behind by the camera (which looks down -Z).
  def lost?(position, center)
    ahead = center.z - position.z
    position.x < @min_x || position.x > @max_x || ahead > @radius * TOO_FAR ||
      -ahead > @radius * BEHIND_SHARE * BEHIND_MARGIN
  end
end
