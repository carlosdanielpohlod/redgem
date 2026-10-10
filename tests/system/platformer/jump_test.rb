# Jumps in place: the hero leaves the ground, rises and lands again.
SETTLE_FRAMES = 30
APEX_FRAMES = 20
LANDING_FRAMES = 120
MIN_JUMP_HEIGHT = 1.0
LANDING_TOLERANCE = 0.05

SystemTest.define do |t|
  player = Scene.find("Player")
  t.wait(SETTLE_FRAMES)
  t.assert("starts grounded") { player.character.grounded? }
  ground_y = player.transform.position.y

  t.press(:space)
  t.wait(APEX_FRAMES)
  t.assert("is airborne") { !player.character.grounded? }
  t.assert("rose at least #{MIN_JUMP_HEIGHT}m") { player.transform.position.y > ground_y + MIN_JUMP_HEIGHT }
  t.screenshot("jump_apex")

  t.wait_until(frames: LANDING_FRAMES, message: "never landed") { player.character.grounded? }
  t.assert("landed back on the ground") { (player.transform.position.y - ground_y).abs < LANDING_TOLERANCE }
end
