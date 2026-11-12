# A test that plays the game, frame by frame, with keys and a gamepad the test holds.
# Run every test of the game with `redgem check . --run-tests` (see the manual's Testing your game).
SETTLE_FRAMES = 30           # the hero lands on the ground first
WALK_FRAMES = 60             # a second
MIN_WALK = 3.0               # meters a second of walking takes it, at least
TURN_FRAMES = 30
MIN_TURN = 30.0              # degrees the stick turns the camera in that time, at least
SIDEWAYS_SHARE = 0.5         # of a walk, how much is across once the camera turned
FULL_TILT = 1.0              # the stick pushed all the way right

SystemTest.define do |t|
  hero = Scene.find!("Player")
  t.wait(SETTLE_FRAMES)
  orbit = Camera.main.get_component(OrbitCamera).orbit

  start = hero.transform.position
  t.hold(:w, frames: WALK_FRAMES)
  walked = hero.transform.position - start
  t.assert("W walks away from the camera") { -walked.z > MIN_WALK }
  t.assert("W walks straight") { walked.x.abs < MIN_WALK * SIDEWAYS_SHARE }

  t.tilt(:right_x, FULL_TILT, frames: TURN_FRAMES)
  t.assert("the right stick turns the camera around the hero") { orbit.yaw.abs > MIN_TURN }

  start = hero.transform.position
  t.hold(:w, frames: WALK_FRAMES)
  walked = hero.transform.position - start
  t.assert("W now walks the way the camera looks") { walked.x.abs > walked.length * SIDEWAYS_SHARE }
end
