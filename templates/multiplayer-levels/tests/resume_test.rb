# Play goes on from the save: the quest comes back as it was, and the party goes to the level it
# was on. Nothing is written over the save before Play puts it in play.
SETTLE_FRAMES = 30
LEVEL_FRAMES = 600          # a change of level, loading screen included
PLAY_BUTTON = 0
SAVED_LEVEL = "level2".freeze
SAVED_QUESTS = { "beacon" => { "stage" => "report", "progress" => [0] } }.freeze

SystemTest.define do |t|
  Save.set(:level, SAVED_LEVEL)
  Save.set(:quests, SAVED_QUESTS)
  t.wait(SETTLE_FRAMES)
  t.assert("the title screen leaves the save alone") { Progress.level == SAVED_LEVEL }

  GameMenu.instance.screens[:title].options[PLAY_BUTTON][1].call
  t.wait_until(frames: LEVEL_FRAMES, message: "Play did not go to the saved level") do
    Game.scene_name == SAVED_LEVEL
  end
  t.wait
  t.assert("the quest is where the save left it") { Quests.stage(:beacon) == "report" }
  t.assert("the save still says that level") { Progress.level == SAVED_LEVEL }
end
