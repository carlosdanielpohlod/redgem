# Driving straight up the first road crosses checkpoint 1, which moves the highlight to checkpoint 2.
SETTLE_FRAMES = 30
MAX_DRIVE_FRAMES = 900
FIRST = 1
SECOND = 2
CHECKPOINTS_IN_LAP = 4

SystemTest.define do |t|
  race = RaceManager.instance
  t.wait(SETTLE_FRAMES)
  t.assert("race starts at checkpoint 1 of 4") do
    race.next_checkpoint == FIRST && race.total_checkpoints == CHECKPOINTS_IN_LAP
  end
  t.assert("only the next banner is shown") do
    Scene.find("Checkpoint1Banner").visible && !Scene.find("Checkpoint2Banner").visible
  end

  t.hold_until(:w, frames: MAX_DRIVE_FRAMES, message: "never reached checkpoint 1") { race.next_checkpoint == SECOND }
  t.assert("banner highlight moved on") do
    !Scene.find("Checkpoint1Banner").visible && Scene.find("Checkpoint2Banner").visible
  end
  t.screenshot("checkpoint_1")
end
