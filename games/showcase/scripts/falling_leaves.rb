# Leaves drifting down the street. Copies of leaf templates fall around the camera, carried by a
# gusty wind and tumbling; a leaf that lands rests for a moment, then starts again from above, so
# there are always leaves in the air wherever the player goes.
# Settings: templates (names of the leaf prefabs to copy), count, radius (m around the camera),
# ceiling (m), rest (seconds a landed leaf lies there), wind_x, wind_z (m/s), min_x, max_x (m, how wide the street is), sidewalk_from (m from
# the middle where the ground is higher), sidewalk_height (m).
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

  # One leaf: its object and how it moves.
  class Leaf
    attr_accessor :object, :position, :fall, :phase, :spin, :rest

    def initialize(object)
      @object = object
      @rest = 0.0
    end
  end

  def start
    @random = Random.new(SEED)
    @radius = prop(:radius, DEFAULT_RADIUS)
    @ceiling = prop(:ceiling, DEFAULT_CEILING)
    @rest = prop(:rest, DEFAULT_REST)
    @wind = Vector3.new(prop(:wind_x, DEFAULT_WIND), 0, prop(:wind_z, 0.0))
    @min_x = prop(:min_x, -DEFAULT_HALF_WIDTH)
    @max_x = prop(:max_x, DEFAULT_HALF_WIDTH)
    @sidewalk_from = prop(:sidewalk_from, DEFAULT_HALF_WIDTH)
    @sidewalk_height = prop(:sidewalk_height, 0.0)
    templates = prop(:templates, [])
    @leaves = []
    return if templates.empty?

    center = view_center
    prop(:count, DEFAULT_COUNT).to_i.times do |index|
      object = Scene.spawn(templates[index % templates.size], position: center)
      next unless object

      leaf = Leaf.new(object)
      launch(leaf, center, between(0.0, @ceiling))  # the first ones are already spread out in the air
      @leaves << leaf
    end
  end

  def update(dt)
    center = view_center
    gust = 1.0 + GUST * Math.sin(Time.elapsed * GUST_SPEED)
    @leaves.each { |leaf| leaf.rest.positive? ? rest(leaf, dt, center) : fall(leaf, dt, center, gust) }
  end

  private

  def view_center
    Camera.main ? Camera.main.transform.position : transform.position
  end

  def between(low, high) = low + (high - low) * @random.rand

  def ground_at(x) = x.abs >= @sidewalk_from ? @sidewalk_height : 0.0

  # Puts a leaf somewhere above the street near the camera (mostly ahead of it: -Z) and gives it
  # its own way of falling.
  def launch(leaf, center, height)
    x = between(@min_x, @max_x)
    z = center.z + between(-@radius, @radius * BEHIND_SHARE)
    leaf.position = Vector3.new(x, [height, ground_at(x) + GROUND_CLEARANCE].max, z)
    leaf.fall = between(MIN_FALL, MAX_FALL)
    leaf.phase = between(0.0, FULL_TURN)
    leaf.spin = Vector3.new(between(-MAX_SPIN, MAX_SPIN), between(-MAX_SPIN, MAX_SPIN), between(-MAX_SPIN, MAX_SPIN))
    leaf.rest = 0.0
    leaf.object.transform.position = leaf.position
    leaf.object.transform.rotation = Vector3.new(between(0.0, FULL_TURN), between(0.0, FULL_TURN), 0)
  end

  def fall(leaf, dt, center, gust)
    swing = Math.sin(Time.elapsed * FLUTTER_SPEED + leaf.phase) * FLUTTER
    drift = @wind * gust + Vector3.new(swing * @wind.z.abs.clamp(0.3, 1.0), 0, swing)
    position = leaf.position + drift * dt - Vector3.up * (leaf.fall * dt)
    ground = ground_at(position.x) + GROUND_CLEARANCE
    if position.y <= ground
      position = Vector3.new(position.x, ground, position.z)
      leaf.rest = @rest
      leaf.object.transform.rotation = Vector3.new(0, between(0.0, FULL_TURN), 0)  # lying flat
    end
    # A leaf the camera left behind comes back at any height: driving fast still has leaves at eye level.
    return launch(leaf, center, between(0.0, @ceiling)) if lost?(position, center)

    leaf.position = position
    leaf.object.transform.position = position
    leaf.object.transform.rotate(leaf.spin * dt) unless leaf.rest.positive?
  end

  def rest(leaf, dt, center)
    leaf.rest -= dt
    return launch(leaf, center, between(0.0, @ceiling)) if lost?(leaf.position, center)

    launch(leaf, center, between(@ceiling * MIN_START_HEIGHT, @ceiling)) unless leaf.rest.positive?
  end

  # Blown off the street, far ahead, or left behind by the camera (which looks down -Z).
  def lost?(position, center)
    ahead = center.z - position.z
    position.x < @min_x || position.x > @max_x || ahead > @radius * TOO_FAR ||
      -ahead > @radius * BEHIND_SHARE * BEHIND_MARGIN
  end
end
