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
ORBIT_FROM_X = 400.0       # pixels: where the mouse drag starts
ORBIT_Y = 300.0
ORBIT_DRAG = 240.0         # pixels dragged sideways: a sixth of a turn around the car
ORBIT_FRAMES = 5
MIN_SWING = 2.0            # meters the camera moves sideways when dragged
ZOOM_NOTCHES = 3.0
MIN_ZOOM = 1.5             # meters further away after those notches
RETURN_FRAMES = 180
MIN_LEAF_TRAVEL = 0.3      # meters a falling leaf covers while the hero drives
MIN_LEAVES_MOVING = 50
STOP_FRAMES = 120          # of hard braking
STOPPED = 0.2              # m/s
FULL_PULL = 1.0            # a trigger pulled, or a stick pushed, all the way
STICK_FRAMES = 30
STEERING_SLIP = 0.4        # the steering stick pulled back a little while the trigger is down

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
  parked_at = car.transform.position
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

  # The mouse looks around: dragging swings the camera to the car's side, the wheel moves it away,
  # and letting go brings it back behind the car.
  behind = camera.transform.position - car.transform.position
  TestInput.move_mouse(ORBIT_FROM_X, ORBIT_Y)
  t.wait
  TestInput.set_mouse_button(:left, true)
  t.wait
  TestInput.move_mouse(ORBIT_FROM_X + ORBIT_DRAG, ORBIT_Y)
  t.wait(ORBIT_FRAMES)
  beside = camera.transform.position - car.transform.position
  t.assert("dragging the mouse swings the camera around the car") { (beside.x - behind.x).abs > MIN_SWING }
  t.screenshot("showcase_look_around")
  TestInput.set_mouse_wheel(-ZOOM_NOTCHES)
  t.wait
  TestInput.set_mouse_wheel(0.0)
  t.wait
  t.assert("the wheel moves the camera away") { camera.transform.position.distance_to(car.transform.position) > beside.length + MIN_ZOOM }
  TestInput.set_mouse_button(:left, false)
  t.wait(RETURN_FRAMES)
  back = camera.transform.position - car.transform.position
  t.assert("letting go brings the camera back behind the car") { (back.x - behind.x).abs < MIN_SWING }

  # Steering hard for a long time cannot leave the road.
  t.hold(:w, :d, frames: STEER_FRAMES)
  t.assert("the curbs keep the car on the road") { car.transform.position.x.abs <= ROAD_HALF_WIDTH }

  # The same car with a gamepad: A stops it, the right trigger drives it, the right stick looks around.
  driver = car.get_component(ArcadeCar)
  t.hold_button(:a, frames: STOP_FRAMES)
  t.assert("A brakes the car to a stop") { driver.speed.abs < STOPPED }
  car.transform.position = parked_at  # away from the curb
  fastest = 0.0
  TestInput.set_gamepad_axis(:right_trigger, FULL_PULL)
  TestInput.set_gamepad_axis(:left_y, STEERING_SLIP)  # the thumb is never exactly centered
  DRIVE_FRAMES.times do
    t.wait
    fastest = [fastest, driver.speed].max
  end
  TestInput.set_gamepad_axis(:right_trigger, 0.0)
  TestInput.set_gamepad_axis(:left_y, 0.0)
  t.assert("the right trigger accelerates (#{fastest} m/s)") { fastest > MIN_SPEED }
  t.wait(RETURN_FRAMES)
  behind = camera.transform.position - car.transform.position
  t.tilt(:right_x, FULL_PULL, frames: STICK_FRAMES)
  beside = camera.transform.position - car.transform.position
  t.assert("the right stick swings the camera around the car") { beside.distance_to(behind) > MIN_SWING }
end
