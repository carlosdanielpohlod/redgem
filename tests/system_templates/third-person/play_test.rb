# A game just made from the third-person template, in a window: the hero walks away from the
# camera, the camera follows behind it, and it jumps.
SETTLE_FRAMES = 30
WALK_FRAMES = 60
JUMP_FRAMES = 15
MIN_WALK = 3.0               # meters a second of walking takes the hero, at least
MOST_BEHIND = 12.0           # meters the camera may be from the hero
GAME_TITLE = "New Game".freeze   # what the test runner called the game (see run_template.sh)

SystemTest.define do |t|
  hero = Scene.find!("Player")
  t.wait(SETTLE_FRAMES)
  t.assert("the game is called what it was created as") { Game.title == GAME_TITLE }
  t.assert("the game starts at once, with no menu open") { !GameMenu.instance.open? }
  t.screenshot("start")

  start = hero.transform.position
  t.hold(:w, frames: WALK_FRAMES)
  t.assert("W walks away from the camera") { start.z - hero.transform.position.z > MIN_WALK }
  behind = Camera.main.transform.position.distance_to(hero.transform.position)
  t.assert("the camera came along") { behind < MOST_BEHIND }
  t.screenshot("walked")

  ground_height = hero.transform.position.y
  t.press(:space)
  t.wait(JUMP_FRAMES)
  t.assert("Space jumps") { hero.transform.position.y > ground_height }
  t.screenshot("jumping")
end
