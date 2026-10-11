# The whole game with a gamepad: the left stick walks (as fast as it is pushed), A jumps, Start
# pauses and A picks the menu's first option.
SETTLE_FRAMES = 30
WALK_FRAMES = 40
JUMP_FRAMES = 10
FULL = -1.0           # the stick pushed all the way forward (up is negative)
HALFWAY = -0.5
MIN_WALK = 1.5        # meters covered by a full push
MIN_JUMP = 0.3
STILL = 0.01
RUNNING = 1.0         # Time.scale while the game goes on
SCALE_TOLERANCE = 0.001

SystemTest.define do |t|
  player = Scene.find("Player")
  menu = GameMenu.instance
  t.wait(SETTLE_FRAMES)
  t.assert("no gamepad until the test plugs one in") { !Input.gamepad? }

  from = player.transform.position
  t.tilt(:left_y, HALFWAY, frames: WALK_FRAMES)
  half = player.transform.position.distance_to(from)
  from = player.transform.position
  t.tilt(:left_y, FULL, frames: WALK_FRAMES)
  full = player.transform.position.distance_to(from)
  t.assert("the stick walks the hero forward") { Input.gamepad? && full > MIN_WALK }
  t.assert("half a push is slower than a full one (#{half} < #{full})") { half.positive? && half < full }

  t.wait(SETTLE_FRAMES)
  ground = player.transform.position.y
  t.press_button(:a)
  t.wait(JUMP_FRAMES)
  t.assert("A jumps") { player.transform.position.y > ground + MIN_JUMP }
  t.wait(SETTLE_FRAMES * 2)

  t.press_button(:start)
  t.wait
  t.assert("Start opens the pause menu") { menu.open? && Time.scale.zero? }
  frozen_at = player.transform.position
  t.tilt(:left_y, FULL, frames: WALK_FRAMES)
  t.assert("the game is frozen while paused") { player.transform.position.distance_to(frozen_at) < STILL }
  t.press_button(:a)
  t.wait
  t.assert("A picks Resume") { !menu.open? && (Time.scale - RUNNING).abs < SCALE_TOLERANCE }
  t.wait(JUMP_FRAMES)
  t.assert("... without making the hero jump") { player.transform.position.y < ground + MIN_JUMP }
end
