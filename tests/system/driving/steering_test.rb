# Holding A while accelerating turns the car left; the chase camera stays behind it.
SETTLE_FRAMES = 60
TURN_FRAMES = 150
MIN_TURN_DEGREES = 20.0
DEGREES_PER_RADIAN = 180.0 / Math::PI
MIN_CAMERA_FACING = 0.8

def heading(object)
  forward = object.transform.forward
  Math.atan2(-forward.x, -forward.z) * DEGREES_PER_RADIAN
end

SystemTest.define do |t|
  car = Scene.find("Car")
  t.wait(SETTLE_FRAMES)
  start_heading = heading(car)

  t.hold(:w, :a, frames: TURN_FRAMES)
  t.assert("car turned left") { heading(car) - start_heading > MIN_TURN_DEGREES }
  to_car = (car.transform.position - Camera.main.transform.position).normalized
  t.assert("camera looks at the car") { Camera.main.transform.forward.dot(to_car) > MIN_CAMERA_FACING }
  t.screenshot("steering")
end
