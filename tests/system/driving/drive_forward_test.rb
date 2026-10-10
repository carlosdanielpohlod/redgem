# Throttle moves the car forward along the road and the wheels follow the chassis.
SETTLE_FRAMES = 60
DRIVE_FRAMES = 120
MIN_SPEED = 5.0
MIN_DISTANCE = 5.0
REST_SPEED = 0.5
# The chassis box is 0.6 m tall: resting on its wheels it must clear the ground by this much.
CHASSIS_HALF_HEIGHT = 0.3
MIN_GROUND_CLEARANCE = 0.15
# A wheel turning at driving speed changes orientation noticeably from one frame to the next.
MAX_SAME_ORIENTATION_DOT = 0.999

SystemTest.define do |t|
  car = Scene.find("Car")
  t.wait(SETTLE_FRAMES)
  t.assert("car rests on its wheels") do
    car.vehicle.speed.abs < REST_SPEED && car.transform.position.y > CHASSIS_HALF_HEIGHT + MIN_GROUND_CLEARANCE
  end
  start = car.transform.position
  t.screenshot("drive_start")

  t.hold(:w, frames: DRIVE_FRAMES)
  t.assert("car is moving forward") { car.vehicle.speed > MIN_SPEED }
  t.assert("car went down the road (-Z)") { car.transform.position.z < start.z - MIN_DISTANCE }
  wheels = %w[WheelFL WheelFR WheelRL WheelRR].map { |name| Scene.find(name) }
  ups = wheels.map { |wheel| wheel.transform.up }
  t.hold(:w, frames: 1)
  t.assert("the simulation spins every wheel") do
    wheels.zip(ups).all? { |wheel, up| wheel.transform.up.dot(up) < MAX_SAME_ORIENTATION_DOT }
  end
  t.screenshot("drive_forward")
end
