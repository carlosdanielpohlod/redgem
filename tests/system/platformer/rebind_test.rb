# A player binds jump to K instead of Space, as a controls screen would: K jumps the hero, Space no
# longer does, and the second player's gamepad stays out of the first player's game.
SETTLE_FRAMES = 30
APEX_FRAMES = 20
LANDING_FRAMES = 120
MIN_JUMP_HEIGHT = 1.0
SECOND_PAD = 1

SystemTest.define do |t|
  player = Scene.find("Player")
  t.wait(SETTLE_FRAMES)
  ground_y = player.transform.position.y
  caught = []
  Input.capture_next { |binding, pad| caught << [binding, pad] }
  t.press(:k)
  t.assert("the capture caught K") { caught == [["key:k", nil]] }
  Input.unbind(:jump, key: :space)
  Input.bind(:jump, caught.first.first)
  t.wait(SETTLE_FRAMES)

  t.press(:space)
  t.wait(APEX_FRAMES)
  t.assert("Space no longer jumps") { player.character.grounded? }
  t.press_button(:a, pad: SECOND_PAD)
  t.wait(APEX_FRAMES)
  t.assert("the second gamepad's A is not the first player's") { player.character.grounded? }

  t.press(:k)
  t.wait(APEX_FRAMES)
  t.assert("K jumps") { player.transform.position.y > ground_y + MIN_JUMP_HEIGHT }
  t.wait_until(frames: LANDING_FRAMES, message: "never landed") { player.character.grounded? }
end
