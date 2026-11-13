# A test that plays the game, frame by frame, with keys the test holds.
# Run every test of the game with `redgem check . --run-tests` (see the manual's Testing your game).
SETTLE_FRAMES = 30           # the hero lands on the ground first
WALK_FRAMES = 60             # a second
MIN_WALK = 3.0               # meters a second of walking takes it, at least
MOST_ASTRAY = 0.5            # meters it may drift off the line it walks along

SystemTest.define do |t|
  hero = Scene.find!("Player")
  t.wait(SETTLE_FRAMES)

  start = hero.transform.position
  t.hold(:d, frames: WALK_FRAMES)
  walked = hero.transform.position - start
  t.assert("D walks right, along +X") { walked.x > MIN_WALK }
  t.assert("D walks straight") { walked.z.abs < MOST_ASTRAY }

  start = hero.transform.position
  t.hold(:w, frames: WALK_FRAMES)
  walked = hero.transform.position - start
  t.assert("W walks up the screen, along -Z") { -walked.z > MIN_WALK }
end
