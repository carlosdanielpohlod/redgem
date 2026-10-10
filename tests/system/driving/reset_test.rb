# R puts the car back on the start line, upright and stopped; falling off the world does too.
# (The vehicle's pitch/roll limit already keeps it from ending upside down.)
SETTLE_FRAMES = 60
DRIVE_FRAMES = 90
UPRIGHT_DOT = 0.9
START_TOLERANCE = 2.0
STOPPED_SPEED = 1.0
BELOW_THE_WORLD = Vector3.new(0, -50, 0)

SystemTest.define do |t|
  car = Scene.find("Car")
  t.wait(SETTLE_FRAMES)
  start = car.transform.position
  back_at_start = -> { car.transform.position.flat.distance_to(start.flat) < START_TOLERANCE }

  t.hold(:w, :d, frames: DRIVE_FRAMES)
  t.assert("car left the start") { !back_at_start.call }

  t.press(:r)
  t.wait(SETTLE_FRAMES)
  t.assert("car is back at the start") { back_at_start.call }
  t.assert("car is upright") { car.transform.up.y > UPRIGHT_DOT }
  t.assert("car is stopped") { car.vehicle.speed.abs < STOPPED_SPEED }

  car.transform.position = BELOW_THE_WORLD
  t.wait(SETTLE_FRAMES)
  t.assert("falling off the world resets the car") { back_at_start.call }
end
