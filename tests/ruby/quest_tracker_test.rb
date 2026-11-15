# Reaching a place can advance the quest while the tracker is looking at its objectives.
class QuestTrackerTest < RedGemTest::Case
  WALK = { "title" => "t", "start" => "path", "stages" => {
    "path" => { "text" => "t", "objectives" => [
      { "go_to" => "Mill", "text" => "t" }, { "collect" => "flour", "text" => "t" }
    ], "next" => "done" },
    "done" => { "text" => "t", "end" => true }
  } }.freeze

  def define_walk
    Quests.__scene_reset
    GameVariables.replace({})
    Quests.instance_variable_get(:@definitions)["walk"] = Quests::Definition.new("walk", WALK)
  end

  def tracker_at_mill
    tracker = QuestTracker.allocate
    def tracker.near?(_objective) = true
    tracker
  end

  def test_a_tracker_reaches_the_last_open_objective_without_reading_the_old_stage_again
    define_walk
    Quests.start(:walk)
    Quests.collected(:flour)
    arrival = Quests[:walk].objectives.first
    tracker_at_mill.__send__(:look_around)
    assert Quests[:walk].complete?
    assert_equal "Mill", arrival.target
    assert_equal 0, arrival.progress
  end
end
