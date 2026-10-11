# Walks forward (W) into the first coin and checks it was collected.
SETTLE_FRAMES = 30
WALK_FRAMES = 60

SystemTest.define do |t|
  t.wait(SETTLE_FRAMES)
  manager = Scene.find("GameManager").get_component(GameManager)
  t.assert("level starts with 5 coins") { manager.total_coins == 5 && manager.coins.zero? }
  t.screenshot("collect_coin_start")

  t.hold(:w, frames: WALK_FRAMES)
  t.wait_until(frames: SETTLE_FRAMES, message: "Coin1 was not collected") { Scene.find("Coin1").nil? }

  t.assert("score counts the coin") { manager.coins == 1 }
  t.assert("the coin bursts into sparks") { Particles.count.positive? }
  t.assert("other coins are untouched") { Scene.find_all(:coin).size == 4 }
  t.screenshot("collect_coin_after")
end
