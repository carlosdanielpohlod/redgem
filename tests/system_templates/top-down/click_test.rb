# A game just made from the top-down template, in a window: a click on the ground walks the hero
# to the spot clicked.
SETTLE_FRAMES = 30
ARRIVE_FRAMES = 240
SPOT = Vector3.new(3, 0, 3)  # a spot of the ground with nothing in the way
NEAR_ENOUGH = 1.0            # meters from the spot the hero may stop (the click lands near it)
GAME_TITLE = "New Game".freeze   # what the test runner called the game (see run_template.sh)

SystemTest.define do |t|
  hero = Scene.find!("Player")
  player = hero.get_component(Player)
  t.wait(SETTLE_FRAMES)
  t.assert("the game is called what it was created as") { Game.title == GAME_TITLE }
  t.screenshot("start")

  x, y = Camera.world_to_screen(SPOT)
  t.click(x, y)
  t.assert("the click picked a spot to walk to") { !player.destination.nil? }
  t.wait_until(frames: ARRIVE_FRAMES, message: "the hero did not get to the spot") { player.destination.nil? }
  t.assert("the hero walked to the spot clicked") { hero.transform.position.flat.distance_to(SPOT) < NEAR_ENOUGH }
  t.screenshot("arrived")
end
