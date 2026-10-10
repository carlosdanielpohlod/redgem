# What happens on the showcase street. The scene file says what is there and where (it is built
# in Blender by tools/blender/build_showcase.py); this says what it all does: who drives, who
# walks, what falls from the trees.
# Distances are in meters, in the engine's axes: X across the street, -Z down the avenue.
class MainScene < SceneScript
  # The street, as it was built.
  ROAD_HALF_WIDTH = 4.5
  CURB_WIDTH = 0.3
  CURB_HEIGHT = 0.15
  SIDEWALK_WIDTH = 4.5
  STREET_ENDS = { min_z: -184.0, max_z: 24.0 }  # where cars turn back or come around

  # Every car is a copy of the same model, whose nose points along its -forward.
  HERO_CAR = "Generic Sport Sedan 2022 PL (placed)"
  CAR_SIZE = { length: 5.3, width: 2.1 }
  CAR_FACES = { reversed: true }
  CURB_CLEARANCE = 1.1                          # from the player's car's center to the curb
  TRAFFIC = {                                   # m/s
    "TrafficBlue" => 10.0, "TrafficWhite" => 12.0, "TrafficBeige" => 11.0,
    "TrafficGrey" => 7.0, "TrafficGreen" => 9.0
  }.freeze
  PARKED_TAG = "parked"

  # People: how far the walkers go before turning back; how the others look around.
  PERSON_FACES = { reversed: true }
  WALKERS = {
    "Wenceslavus (placed)" => 16.0,
    "Luke (placed)" => 13.0,
    "Standing Survivor Girl Green Top Cowboy (placed)" => 22.0,
    "Axel Standing (placed)" => 24.0
  }.freeze
  BYSTANDERS = {
    "Standing Cute Girl White Jacket Jeans (placed)" => { look: 7.0, pace: 8.0 },
    "Andrew (placed)" => { look: 4.0, pace: 9.0 },
    "Charles (placed)" => { look: 6.0, pace: 7.0 }
  }.freeze

  LEAF_TEMPLATES = %w[Leaf1 Leaf2 Leaf3 Leaf4 Leaf5].freeze
  LEAVES = { count: 420, radius: 20.0, ceiling: 9.0, rest: 12.0, wind_x: 1.1, wind_z: 0.6 }.freeze

  # The crowd: copies of those people all along both sidewalks, one spot every few meters down
  # the street. Each spot is clear of the lamps, trees and street furniture, which repeat every
  # LAMP_SPACING from the street's first lamp.
  CROWD_SEED = 7
  FIRST_LAMP_Z = 30.0
  LAMP_SPACING = 18.0
  CROWD_SPOTS = [2.2, 11.2].freeze              # meters down the street from each lamp
  CROWD_FROM_Z = -40.0                          # nearer than this, the people placed by hand
  CROWD_TO_Z = -180.0
  WALKER_SHARE = 0.6                            # the rest stand around
  WALKING_LANES = { 0.0 => 7.9, 180.0 => 7.2 }.freeze  # yaw the walker faces => distance from the middle
  WALK_DISTANCES = [10.0, 26.0].freeze          # range, m
  STANDING_AT = 6.9                             # distance from the middle of the street
  FACING_THE_ROAD = 90.0                        # yaw, on the left-hand sidewalk (mirrored on the other)
  GLANCE = 50.0                                 # degrees a bystander may be turned away from the road
  LOOKS = [3.0, 8.0].freeze                     # how bystanders look around: degrees ...
  PACES = [6.0, 10.0].freeze                    # ... and seconds per cycle
  WALK_CYCLE = 1.25                             # seconds: copies start out of step within it

  def setup
    drive_cars
    move_people
    drop_leaves
  end

  private

  def drive_cars
    hero = find!(HERO_CAR)
    side = ROAD_HALF_WIDTH - CURB_CLEARANCE
    hero.add_component(CarBody, **CAR_SIZE)
    hero.add_component(ArcadeCar, min_x: -side, max_x: side, **STREET_ENDS, **CAR_FACES)
    hero.add_component(SpinningWheels, **CAR_FACES)
    Camera.main.add_component(ChaseCamera, target: hero)

    TRAFFIC.each do |name, speed|
      car = find!(name)
      car.add_component(CarBody, **CAR_SIZE)
      car.add_component(TrafficCar, speed: speed, min_x: -side, max_x: side, **STREET_ENDS, **CAR_FACES)
      car.add_component(SpinningWheels, **CAR_FACES)
    end
    find_all(PARKED_TAG).each { |car| car.add_component(CarBody, **CAR_SIZE) }
  end

  def move_people
    WALKERS.each { |name, distance| find!(name).add_component(Pedestrian, distance: distance, **PERSON_FACES) }
    BYSTANDERS.each { |name, manner| find!(name).add_component(IdlePerson, **manner) }
    fill_the_sidewalks
  end

  def fill_the_sidewalks
    @random = Random.new(CROWD_SEED)
    walkers = WALKERS.keys.map { |name| find!(name) }
    bystanders = BYSTANDERS.keys.map { |name| find!(name) }
    crowd_spots.each_with_index do |(side, z), index|
      if @random.rand < WALKER_SHARE
        add_walker(walkers[index % walkers.size], side, z)
      else
        add_bystander(bystanders[index % bystanders.size], side, z)
      end
    end
  end

  # [side (-1 left, 1 right), z] of every spot of the crowd.
  def crowd_spots
    spots = []
    lamp_z = FIRST_LAMP_Z
    while lamp_z > CROWD_TO_Z
      CROWD_SPOTS.each do |offset|
        z = lamp_z - offset
        [-1.0, 1.0].each { |side| spots << [side, z] } if z <= CROWD_FROM_Z && z >= CROWD_TO_Z
      end
      lamp_z -= LAMP_SPACING
    end
    spots
  end

  def between(range) = range[0] + (range[1] - range[0]) * @random.rand

  def add_walker(original, side, z)
    yaw = WALKING_LANES.keys[@random.rand < 0.5 ? 0 : 1]
    walker = copy_of(original, side * WALKING_LANES[yaw], z, yaw)
    walker.animation_offset = @random.rand * WALK_CYCLE
    walker.add_component(Pedestrian, distance: between(WALK_DISTANCES), **PERSON_FACES)
  end

  def add_bystander(original, side, z)
    yaw = -side * FACING_THE_ROAD + (@random.rand * 2.0 - 1.0) * GLANCE
    copy_of(original, side * STANDING_AT, z, yaw).add_component(IdlePerson, look: between(LOOKS), pace: between(PACES))
  end

  def copy_of(original, x, z, yaw)
    Scene.spawn(original, position: Vector3.new(x, original.transform.position.y, z), rotation: Vector3.new(0, yaw, 0))
  end

  # The leaves fall around the camera, wherever it goes.
  def drop_leaves
    street_half_width = ROAD_HALF_WIDTH + CURB_WIDTH + SIDEWALK_WIDTH
    Camera.main.add_component(FallingLeaves, templates: LEAF_TEMPLATES, min_x: -street_half_width,
                                             max_x: street_half_width, sidewalk_from: ROAD_HALF_WIDTH + CURB_WIDTH,
                                             sidewalk_height: CURB_HEIGHT, **LEAVES)
  end
end
