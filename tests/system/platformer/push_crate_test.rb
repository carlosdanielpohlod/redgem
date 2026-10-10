# The hero walks into the stacked dynamic crates and pushes them.
SETTLE_FRAMES = 30
PUSH_FRAMES = 120
MIN_PUSH = 0.3
RESTING_SPEED = 0.1
# Left of the crates (Blender x = 4, y = 3 -> engine x = 4, z = -3), lined up to push them along +X.
PUSH_START_X = 2.0
CRATE_LANE_Z = -3.0

SystemTest.define do |t|
  crate = Scene.find("Crate1")
  player = Scene.find("Player")
  t.wait(SETTLE_FRAMES)
  start = crate.transform.position
  t.assert("crate rests on the ground") { crate.rigidbody.velocity.length < RESTING_SPEED }

  player.transform.position = Vector3.new(PUSH_START_X, player.transform.position.y, CRATE_LANE_Z)
  t.hold(:d, frames: PUSH_FRAMES)
  t.assert("crate was pushed to the right") { crate.transform.position.x > start.x + MIN_PUSH }
  t.screenshot("push_crate")
end
