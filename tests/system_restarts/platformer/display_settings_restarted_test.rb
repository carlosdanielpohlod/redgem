# The game started again after tests/system/platformer/display_settings_test.rb: the window comes
# back at the size chosen, the quality as it was lowered, the interface as much bigger as it was
# made, and jump as the controls page bound it
# (K and Y, no longer Space and A).
SETTLE_FRAMES = 30
RESIZE_FRAMES = 60
APEX_FRAMES = 20
LANDING_FRAMES = 120
MIN_JUMP_HEIGHT = 1.0
SMALL = [640, 360].freeze
LARGER_UI = 1.25
HALF = 0.5                  # the pixels a unit of the platformer's interface takes in the small window
SCALE_TOLERANCE = 0.001

SystemTest.define do |t|
  player = Scene.find("Player")
  t.wait(SETTLE_FRAMES)
  t.assert("the bindings are the ones the controls page made") { Input.bindings(:jump) == ["key:k", "pad:y"] }
  t.assert("the quality is the one chosen") { Settings.quality == :high }
  t.wait_until(frames: RESIZE_FRAMES, message: "the window is not the size chosen") do
    [UI.pixel_width, UI.pixel_height] == SMALL
  end
  t.assert("the interface is as much bigger as the player made it") do
    (Settings.ui_scale - LARGER_UI).abs < SCALE_TOLERANCE
  end
  t.assert("which the interface's scale is multiplied by") { (UI.scale - HALF * LARGER_UI).abs < SCALE_TOLERANCE }

  ground_y = player.transform.position.y
  t.press(:space)
  t.wait(APEX_FRAMES)
  t.assert("Space still does not jump") { player.character.grounded? }
  t.press_button(:y)
  t.wait(APEX_FRAMES)
  t.assert("Y jumps") { player.transform.position.y > ground_y + MIN_JUMP_HEIGHT }
  t.wait_until(frames: LANDING_FRAMES, message: "never landed") { player.character.grounded? }
  t.press(:k)
  t.wait(APEX_FRAMES)
  t.assert("K jumps") { player.transform.position.y > ground_y + MIN_JUMP_HEIGHT }
end
