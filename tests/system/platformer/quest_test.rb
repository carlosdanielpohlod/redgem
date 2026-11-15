# The hero plays the old tree's quest (games/platformer/quests/tree.json) through its two stages:
# watering the tree in its dialogue moves it on, and a coin and the finish line complete it.
SETTLE_FRAMES = 30
PICKUP_FRAMES = 30
REACH_FRAMES = 30
REACH = 2.0               # meters from the hero to the tree's middle
# Beside the first tree (Blender x = -5.5, y = 1 -> engine x = -5.5, z = -1).
BESIDE_TREE_X = -4.0
TREE_LANE_Z = -1.0

SystemTest.define do |t|
  player = Scene.find("Player")
  tree = Scene.find("Tree1")
  t.wait(SETTLE_FRAMES)
  tree.add_component(Talkable, dialogue: "tree", range: REACH)
  player.add_component(QuestTracker)
  heard = []
  Quests.on(:stage) { |quest, stage, _from| heard << [quest.name, stage] }
  completed = []
  Quests.on(:complete) { |quest| completed << quest.name }
  t.assert("the quest starts") { Quests.start(:tree) }
  quest = Quests[:tree]
  t.assert("its first stage asks to water the tree") { quest.objectives.map(&:text) == ["Water the old tree"] }

  player.transform.position = Vector3.new(BESIDE_TREE_X, player.transform.position.y, TREE_LANE_Z)
  t.wait
  t.assert("the hero talks to the tree") { Interaction.interact(player, tree) && Dialogue.current }
  t.assert("watering it takes the answer") { Dialogue.current.choose(0) }
  t.assert("the tree's thanks moved the quest on") { quest.stage == "goal" }
  t.assert("the second stage counts from nothing") { quest.objectives.map(&:text).first == "Collect a coin (0/1)" }
  Dialogue.current.continue

  coin = Scene.find("Coin1")
  player.transform.position = coin.transform.position
  t.wait_until(frames: PICKUP_FRAMES, message: "Coin1 was not collected") { !coin.alive? }
  t.assert("the coin counted") { quest.objectives.first.done? && quest.stage == "goal" }
  player.transform.position = Scene.find("Goal").transform.position
  t.wait_until(frames: REACH_FRAMES, message: "the finish line was not reached") { quest.complete? }
  t.assert("the game heard of both stages") { heard == [%w[tree water], %w[tree goal], %w[tree done]] }
  t.assert("and of its end") { completed == ["tree"] }
  Quests.save
  t.assert("it is saved as it is") { Save[:quests]["tree"]["stage"] == "done" }
end
