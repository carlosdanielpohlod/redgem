# The system test tools/release/smoke.sh plays in a window, on a new third-person game: the hero
# stands on the ground, Space lifts it, and the frame is kept as a picture (smoke_window.png).
SETTLE_FRAMES = 60           # the hero lands on the ground first
RISE_FRAMES = 15             # a quarter of a second after the jump

SystemTest.define do |t|
  hero = Scene.find!("Player")
  t.wait_until(frames: SETTLE_FRAMES, message: "the hero never stood on the ground") { hero.character.grounded? }
  ground_height = hero.transform.position.y
  t.press(:space)
  t.wait(RISE_FRAMES)
  t.assert("Space jumps") { hero.transform.position.y > ground_height }
  t.screenshot("smoke_window")
end
