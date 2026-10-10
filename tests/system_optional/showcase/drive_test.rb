# The real-time export of the photoreal street is playable: W drives the car down the avenue,
# the camera chases it, and the curbs keep it on the road.
WARMUP_FRAMES = 30
DRIVE_FRAMES = 180
STEER_FRAMES = 240
MIN_DRIVE = 8.0          # meters covered while accelerating
MIN_SPEED = 3.0          # m/s
CHASE_MAX_DISTANCE = 14.0
MIN_OBJECTS = 100
ROAD_HALF_WIDTH = 4.5
CAR_NAME = "Generic Sport Sedan 2022 PL (placed)"
TRAFFIC_NAME = "TrafficBlue"
WALKER_NAME = "Wenceslavus (placed)"
WALKER_BONE = "Thigh.L"
MIN_TRAFFIC_TRAVEL = 5.0   # meters while the hero drives
MIN_WALK = 0.5
MIN_WHEEL_TURN = 5.0       # degrees
MIN_BONE_SWING = 0.01      # meters a leg bone moves relative to the walker
MORE_WALKERS = ["Standing Survivor Girl Green Top Cowboy (placed)", "Axel Standing (placed)"]
BYSTANDER_NAME = "Standing Cute Girl White Jacket Jeans (placed)"
LEAF_PREFIX = "Leaf"
MIN_LEAVES = 100
MIN_LEAF_TRAVEL = 0.3      # meters a falling leaf covers while the hero drives
MIN_LEAVES_MOVING = 50

SystemTest.define do |t|
  car = Scene.find(CAR_NAME)
  camera = Camera.main
  t.assert("the street is populated") { Scene.objects.size > MIN_OBJECTS }
  t.assert("the car is there to drive") { !car.nil? }
  traffic = Scene.find(TRAFFIC_NAME)
  walker = Scene.find(WALKER_NAME)
  bone = Scene.find(WALKER_BONE)
  wheel = car.children.find { |child| child.name.include?("Wheel") }
  t.assert("traffic, a pedestrian with a skeleton and wheels are there") { traffic && walker && bone && wheel }
  walkers = MORE_WALKERS.map { |name| Scene.find(name) }
  t.assert("more people are on the sidewalks") { walkers.all? && !Scene.find(BYSTANDER_NAME).nil? }
  t.wait(WARMUP_FRAMES)
  leaves = Scene.objects.select { |object| object.name.start_with?(LEAF_PREFIX) }
  t.assert("leaves are in the air") { leaves.size >= MIN_LEAVES }
  leaf_starts = leaves.map { |leaf| leaf.transform.position }
  walker_starts = walkers.map { |person| person.transform.position }
  start = car.transform.position
  traffic_start = traffic.transform.position
  walker_start = walker.transform.position
  bone_offset = bone.transform.position - walker_start
  wheel_start = wheel.transform.rotation
  t.screenshot("showcase_start")

  t.hold(:w, frames: DRIVE_FRAMES)
  t.assert("W drives the car down the avenue") { start.z - car.transform.position.z > MIN_DRIVE }
  t.assert("the car has speed") { car.get_component(ArcadeCar).speed > MIN_SPEED }
  t.assert("the camera chases the car") { camera.transform.position.distance_to(car.transform.position) < CHASE_MAX_DISTANCE }
  t.screenshot("showcase_driving")
  t.assert("other cars drive along the street") { traffic.transform.position.distance_to(traffic_start) > MIN_TRAFFIC_TRAVEL }
  t.assert("the pedestrian walks") { walker.transform.position.distance_to(walker_start) > MIN_WALK }
  t.assert("the other pedestrians walk too") do
    walkers.each_with_index.all? { |person, index| person.transform.position.distance_to(walker_starts[index]) > MIN_WALK }
  end
  t.assert("the leaves drift") do
    moved = leaves.each_with_index.count { |leaf, index| leaf.transform.position.distance_to(leaf_starts[index]) > MIN_LEAF_TRAVEL }
    moved >= MIN_LEAVES_MOVING
  end
  t.assert("the walk animation moves the leg bones") do
    (bone.transform.position - walker.transform.position).distance_to(bone_offset) > MIN_BONE_SWING
  end
  t.assert("the wheels turn") { wheel.transform.rotation.distance_to(wheel_start) > MIN_WHEEL_TURN }

  # Steering hard for a long time cannot leave the road.
  t.hold(:w, :d, frames: STEER_FRAMES)
  t.assert("the curbs keep the car on the road") { car.transform.position.x.abs <= ROAD_HALF_WIDTH }
end
