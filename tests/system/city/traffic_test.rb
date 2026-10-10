# The city runs on its own: cars drive, lights alternate, cars stop at red, the camera orbits.
WARMUP_FRAMES = 30
OBSERVE_FRAMES = 1080  # 18 s: longer than a full avenue + street light cycle
MIN_DRIVEN = 5.0
MIN_CAMERA_TRAVEL = 2.0
EXPECTED_MOVING_CARS = 8
STOPPED_SPEED = 0.1
WHEELS_PER_CAR = 4

SystemTest.define do |t|
  cars = Scene.find_all(:car)
  t.assert("#{EXPECTED_MOVING_CARS} moving cars") { cars.size == EXPECTED_MOVING_CARS }
  controller = TrafficController.instance
  camera = Camera.main
  t.wait(WARMUP_FRAMES)
  t.screenshot("city_start")

  starts = cars.map { |car| car.transform.position }
  camera_start = camera.transform.position
  phases = []
  stopped_at_red = false
  OBSERVE_FRAMES.times do
    phases << controller.green_axis
    stopped_at_red ||= cars.any? { |car| car.get_component(TrafficCar).speed < STOPPED_SPEED }
    t.wait
  end
  t.screenshot("city_later")

  driven = cars.zip(starts).count { |car, start| car.transform.position.distance_to(start) > MIN_DRIVEN }
  t.assert("most cars drove") { driven > cars.size / 2 }
  t.assert("both roads got a green light") { phases.include?("avenue") && phases.include?("street") }
  t.assert("some car waited") { stopped_at_red }
  t.assert("the camera orbits") { camera.transform.position.distance_to(camera_start) > MIN_CAMERA_TRAVEL }
  t.assert("every car rolls on four wheels (spare tires excluded)") do
    cars.all? { |car| car.children.count { |child| TrafficCar.rolling_wheel?(child) } == WHEELS_PER_CAR }
  end
end
