# End to end: the goal refuses to finish early, then every coin is collected and the game is won.
SETTLE_FRAMES = 20
PICKUP_FRAMES = 30
GOAL_FRAMES = 60
OUTSIDE_THE_GOAL = Vector3.new(0, 0, 4)

SystemTest.define do |t|
  player = Scene.find("Player")
  manager = Scene.find("GameManager").get_component(GameManager)
  goal = Scene.find("Goal")
  t.wait(SETTLE_FRAMES)

  player.transform.position = goal.transform.position
  t.wait(GOAL_FRAMES)
  t.assert("cannot win without the coins") { !manager.won? }

  Scene.find_all(:coin).each do |coin|
    player.transform.position = coin.transform.position
    t.wait_until(frames: PICKUP_FRAMES, message: "#{coin.name} not collected") { !coin.alive? }
  end
  t.assert("all coins collected") { manager.all_coins? && manager.coins == manager.total_coins }

  # Leave the trigger and come back so the goal sees a fresh entry.
  player.transform.position = goal.transform.position + OUTSIDE_THE_GOAL
  t.wait(SETTLE_FRAMES)
  player.transform.position = goal.transform.position
  t.wait_until(frames: GOAL_FRAMES, message: "goal did not register") { manager.won? }
  t.screenshot("win_game")
end
