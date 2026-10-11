# P opens the pause menu and freezes the game; clicking Resume lets it go on.
SETTLE_FRAMES = 30
WALK_FRAMES = 30
MIN_WALK = 1.0
STILL = 0.01
RUNNING = 1.0   # Time.scale while the game goes on
SCALE_TOLERANCE = 0.001
RESUME_BUTTON = 0

SystemTest.define do |t|
  player = Scene.find("Player")
  menu = GameMenu.instance
  t.assert("the platformer has a menu, closed while playing") { menu && !menu.open? }
  t.wait(SETTLE_FRAMES)

  t.press(:p)
  t.wait
  t.assert("P opens the pause menu") { menu.open? && Time.scale.zero? }
  frozen_at = player.transform.position
  t.hold(:w, frames: WALK_FRAMES)
  t.assert("the game is frozen while paused") { player.transform.position.distance_to(frozen_at) < STILL }
  t.screenshot("pause_menu")

  x, y, width, height = menu.button_box(RESUME_BUTTON)
  t.click(x + width / 2, y + height / 2)
  t.assert("clicking Resume closes the menu") { !menu.open? && (Time.scale - RUNNING).abs < SCALE_TOLERANCE }
  t.hold(:w, frames: WALK_FRAMES)
  t.assert("the game goes on") { player.transform.position.distance_to(frozen_at) > MIN_WALK }
end
