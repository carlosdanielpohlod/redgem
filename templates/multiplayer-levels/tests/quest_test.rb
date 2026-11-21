# Plays the whole quest alone, through both levels: the guide gives it, the door takes the hero to
# the second level, where the beacon is, and back, and the guide hears of it. The save keeps the
# level and the quest at every step, across each change of level.
# Run every test of the game with `redgem check . --run-tests` (see the manual's Testing your game).
SETTLE_FRAMES = 30
TALK_FRAMES = 30            # the Interactor picks the guide up, the conversation starts
LEVEL_FRAMES = 600          # a change of level, loading screen included
QUEST_FRAMES = 120
NEAR = Vector3.new(0, 0, 1.5)   # where the hero stands to use something: in front of it, in reach
PLAY_BUTTON = 0

SystemTest.define do |t|
  Progress.clear
  t.wait(SETTLE_FRAMES)
  menu = GameMenu.instance
  t.assert("the game opens on its title screen") { menu.screen == :title }
  menu.screens[:title].options[PLAY_BUTTON][1].call
  t.assert("Play starts the game") { !menu.open? }
  t.assert("the hero starts on Spawn1") do
    Scene.find("Player").transform.position.flat.distance_to(Scene.find("Spawn1").transform.position.flat) < 0.1
  end

  # Walks the hero up to something and presses E.
  use = lambda do |name|
    Scene.find("Player").transform.position = Scene.find(name).transform.position + NEAR
    t.wait
    t.press(:e)
  end
  # Goes through the conversation, taking the first choice it offers.
  talk = lambda do
    use.call("Guide")
    t.wait_until(frames: TALK_FRAMES, message: "no conversation started") { Dialogue.current }
    Dialogue.current.choosing? ? Dialogue.current.choose(0) : Dialogue.current.continue while Dialogue.current
  end

  talk.call
  t.assert("the guide gave the quest") { Quests.stage(:beacon) == "find" }
  t.assert("the save has it") { Save.get(:quests)["beacon"]["stage"] == "find" }

  use.call("Exit")
  t.wait_until(frames: LEVEL_FRAMES, message: "the door did not lead to level2") { Game.scene_name == "level2" }
  t.wait
  t.assert("the quest came along") { Quests.stage(:beacon) == "find" }
  t.assert("the save kept the quest and says the new level") do
    Progress.level == "level2" && Save.get(:quests)["beacon"]["stage"] == "find"
  end
  t.screenshot("level2")

  Scene.find("Player").transform.position = Scene.find("Beacon").transform.position + NEAR
  t.wait_until(frames: QUEST_FRAMES, message: "the beacon was not reached") { Quests.stage(:beacon) == "report" }

  use.call("Exit")
  t.wait_until(frames: LEVEL_FRAMES, message: "the door did not lead back") { Game.scene_name == "main" }
  t.wait
  talk.call
  t.assert("telling the guide completes the quest") { Quests[:beacon].complete? }
  t.assert("the save says it is done, on the first level") do
    Progress.level == "main" && Save.get(:quests)["beacon"]["stage"] == "done"
  end
end
