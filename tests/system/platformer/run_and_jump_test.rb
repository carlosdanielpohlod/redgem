# Jumping while running (the hero turns to face its movement every frame) still reaches full height.
SETTLE_FRAMES = 30
RUN_UP_FRAMES = 10
APEX_FRAMES = 20
MIN_JUMP_HEIGHT = 1.0

SystemTest.define do |t|
  player = Scene.find("Player")
  t.wait(SETTLE_FRAMES)
  ground_y = player.transform.position.y

  TestInput.set(:a, true)
  t.wait(RUN_UP_FRAMES)
  t.press(:space)
  t.wait(APEX_FRAMES)
  TestInput.set(:a, false)
  t.assert("jumped at least #{MIN_JUMP_HEIGHT}m while running") do
    player.transform.position.y > ground_y + MIN_JUMP_HEIGHT
  end
  t.screenshot("run_and_jump")
end
