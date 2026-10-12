# A game just made from the empty template: its cube spins, and it has the title it was given.
SETTLE_FRAMES = 10
SPIN_FRAMES = 60
MIN_TURN = 10.0                  # degrees in a second of spinning
GAME_TITLE = "New Game".freeze   # what the test runner called the game (see run_template.sh)

SystemTest.define do |t|
  cube = Scene.find("Cube")
  t.wait(SETTLE_FRAMES)
  t.assert("the game is called what it was created as") { Game.title == GAME_TITLE }
  before = cube.transform.rotation.y
  t.wait(SPIN_FRAMES)
  t.assert("the cube spins") { (cube.transform.rotation.y - before).abs > MIN_TURN }
  t.screenshot("spinning_cube")
end
