# A test that plays the jump: Space lifts the hero off the ground, and it lands again.
SETTLE_FRAMES = 60           # the hero lands on the ground first
RISE_FRAMES = 15             # a quarter of a second after the jump
MIN_RISE = 0.5               # meters it is up by then, at least
LAND_FRAMES = 120

SystemTest.define do |t|
  hero = Scene.find!("Player")
  t.wait_until(frames: SETTLE_FRAMES, message: "the hero never stood on the ground") { hero.character.grounded? }

  ground_height = hero.transform.position.y
  t.press(:space)
  t.wait(RISE_FRAMES)
  t.assert("Space jumps") { hero.transform.position.y > ground_height + MIN_RISE }
  t.wait_until(frames: LAND_FRAMES, message: "the hero did not land") { hero.character.grounded? }
end
