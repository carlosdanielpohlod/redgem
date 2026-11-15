# Quests as one machine has them, offline (quests.rb): where a quest starts, what counts for its
# objectives, how it goes on from stage to stage, who hears of it, and how it is saved. The quest
# is given as the engine reads its file (string keys); the people in it are stand-ins.

# Someone of the scene, as the quests and a conversation see them.
class Someone
  attr_reader :name

  def initialize(name)
    @name = name
  end

  def get_component(_klass) = nil
end

class QuestsTest < RedGemTest::Case
  SACKS = 3
  # Bread for the baker: collect flour and drive off a wolf, bring the flour and talk to the baker,
  # then wait for the oven.
  BREAD = {
    "title" => "bread.title", "start" => "flour",
    "stages" => {
      "flour" => { "text" => "bread.flour", "objectives" => [
        { "collect" => "flour", "count" => SACKS, "text" => "bread.collect" },
        { "defeat" => "Wolf", "text" => "bread.wolf" }
      ], "next" => "bring" },
      "bring" => { "text" => "bread.bring", "objectives" => [
        { "deliver" => "flour", "to" => "Baker", "count" => SACKS, "text" => "bread.deliver" },
        { "talk_to" => "Baker", "dialogue" => "baker", "node" => "thanks", "text" => "bread.talk" }
      ], "next" => "bake" },
      "bake" => { "text" => "bread.bake", "objectives" => [{ "check" => "oven_hot", "text" => "bread.oven" }],
                  "next" => "done" },
      "waiting" => { "text" => "bread.waiting" },
      "done" => { "text" => "bread.done", "end" => true }
    }
  }.freeze
  BAKER = { "start" => "hello", "nodes" => {
    "hello" => { "text" => "baker.hello", "next" => "thanks" },
    "thanks" => { "text" => "baker.thanks", "end" => true }
  } }.freeze
  # Two stages that pass each other on at once, for ever.
  ROUND = { "title" => "t", "start" => "a", "stages" => {
    "a" => { "text" => "t", "objectives" => [{ "check" => "always", "text" => "t" }], "next" => "b" },
    "b" => { "text" => "t", "objectives" => [{ "check" => "always", "text" => "t" }], "next" => "a" }
  } }.freeze
  WALK = { "title" => "t", "start" => "path", "stages" => {
    "path" => { "text" => "t", "objectives" => [
      { "go_to" => "Mill", "text" => "t" }, { "collect" => "flour", "text" => "t" }
    ], "next" => "done" },
    "done" => { "text" => "t", "end" => true }
  } }.freeze

  def setup
    [Quests, Dialogue, GameVariables].each(&:__scene_reset)
    Dialogue.quests = nil
    GameVariables.replace({})
    define_quest("bread", BREAD)
    Dialogue.instance_variable_get(:@scripts)["baker"] = Dialogue::Script.new("baker", BAKER)
    @heard = []
    Quests::EVENTS.each { |event| Quests.on(event) { |quest, *rest| @heard << [event, quest.name, *rest] } }
  end

  def without_file(name) = Quests.instance_variable_get(:@definitions)[name] = false

  def define_quest(name, data)
    Quests.instance_variable_get(:@definitions)[name] = Quests::Definition.new(name, data)
  end

  def bread = Quests[:bread]

  def heard(event) = @heard.select { |line| line[0] == event }.map { |line| line[1..] }

  # The first stage done: the flour collected and the wolf gone.
  def through_flour
    Quests.start(:bread)
    Quests.collected(:flour, SACKS)
    Quests.defeated(Someone.new("Wolf"))
  end

  def test_a_quest_starts_at_its_first_stage
    refute bread.started?
    assert Quests.start(:bread)
    assert_equal "flour", bread.stage
    assert bread.active?
    assert_equal "bread.title", bread.title, "a key with no text shows as itself"
    assert_equal %i[collect defeat], bread.objectives.map(&:kind)
  end

  def test_a_quest_starts_once_and_the_game_hears_of_it
    Quests.start(:bread)
    refute Quests.start(:bread), "it had started"
    assert_equal [["bread"]], heard(:start)
    assert_equal [["bread", "flour", nil]], heard(:stage)
  end

  def test_collecting_counts_up_to_what_the_objective_asks
    Quests.start(:bread)
    Quests.collected(:flour)
    sacks = bread.objectives.first
    assert_equal 1, sacks.progress
    Quests.collected("flour", SACKS)
    assert_equal SACKS, sacks.progress, "it stops at its count"
    assert sacks.done?
    assert_equal([%w[bread], %w[bread]], heard(:objective).map { |line| line[0..0] })
  end

  def test_what_is_not_asked_for_counts_for_nothing
    Quests.collected(:flour)
    Quests.start(:bread)
    Quests.collected(:salt)
    Quests.defeated(Someone.new("Wolfhound"))
    assert_equal [0, 0], bread.objectives.map(&:progress)
  end

  def test_once_every_objective_is_done_the_quest_goes_on
    through_flour
    assert_equal "bring", bread.stage
    assert_equal [0, 0], bread.objectives.map(&:progress), "the next stage starts from nothing"
    assert_equal %w[bread bring flour], heard(:stage).last
  end

  def test_a_tracker_reaches_the_last_open_objective_without_reading_the_old_stage_again
    define_quest("walk", WALK)
    Quests.start(:walk)
    Quests.collected(:flour)
    arrival = Quests[:walk].objectives.first
    tracker = QuestTracker.allocate
    def tracker.near?(_objective) = true
    tracker.__send__(:look_around)
    assert Quests[:walk].complete?
    assert_equal "Mill", arrival.target
    assert_equal 0, arrival.progress
  end

  def test_a_copy_of_someone_counts_as_them
    Quests.start(:bread)
    Quests.defeated(Someone.new("Wolf.003"))
    assert bread.objectives.last.done?
  end

  def test_a_delivery_counts_only_to_whom_it_goes
    through_flour
    Quests.delivered(:flour, to: Someone.new("Miller"), count: SACKS)
    assert_equal 0, bread.objectives.first.progress
    Quests.delivered(:flour, to: Someone.new("Baker.001"), count: SACKS)
    assert bread.objectives.first.done?
  end

  def test_talking_to_someone_counts_at_the_node_it_asks_for
    through_flour
    Quests.delivered(:flour, to: "Baker", count: SACKS)
    conversation = Dialogue.start("baker", actor: Someone.new("Hero"), speaker: Someone.new("Baker"))
    refute bread.objectives.last.done?, "not at its node yet"
    conversation.continue
    assert_equal "bake", bread.stage
  end

  def test_a_dialogue_moves_a_quest_and_reads_its_stage
    Quests.start(:bread)
    Dialogue::Rules.apply([{ "quest" => "bread", "to" => "waiting" }], nil)
    assert_equal "waiting", Dialogue.quests.stage("bread")
    assert Dialogue::Rules.hold?([{ "quest" => "bread", "is" => "waiting" }])
    assert_raises(ArgumentError) { Dialogue::Rules.apply([{ "quest" => "bread", "to" => "burnt" }], nil) }
  end

  def test_a_check_of_the_game_s_own_moves_it_on
    Quests.set_stage(:bread, "bake")
    hot = false
    Quests.check(:oven_hot) { |quest| hot && quest.name == "bread" }
    Quests.__poll
    assert_equal "bake", bread.stage
    hot = true
    Quests.__poll
    assert bread.complete?
  end

  def test_a_quest_that_ends_is_complete
    Quests.set_stage(:bread, "done")
    refute bread.active?
    assert_equal [["bread"]], heard(:complete)
    assert_equal ["bread"], Quests.completed.map(&:name)
  end

  def test_a_check_already_passed_counts_as_its_stage_starts
    Quests.check(:oven_hot) { |_quest| true }
    Quests.set_stage(:bread, "bake")
    assert bread.complete?
  end

  def test_stages_that_go_round_raise
    define_quest("round", ROUND)
    Quests.check(:always) { |_quest| true }
    assert_raises(ArgumentError) { Quests.start(:round) }
  end

  def test_a_stage_the_quest_has_not_raises
    error = assert_raises(ArgumentError) { Quests.set_stage(:bread, "burnt") }
    assert error.message.include?("bake")
  end

  def test_a_quest_with_no_file_is_only_its_stage
    without_file("errand")
    Quests.set_stage(:errand, "asked")
    assert_equal "asked", Quests.stage(:errand)
    assert_equal({ "stage" => "asked", "progress" => [] }, Quests.to_h["errand"])
  end

  def test_quests_are_saved_as_they_are
    through_flour
    Quests.delivered(:flour, to: "Baker")
    assert_equal({ "bread" => { "stage" => "bring", "progress" => [1, 0] } }, Quests.to_h)
    Quests.reset(:bread)
    assert_equal({}, Quests.to_h)
  end

  def test_quests_are_put_back_and_only_their_change_is_heard
    Quests.replace("bread" => { "stage" => "bring", "progress" => [1, 0] })
    assert_equal "bring", bread.stage
    assert_equal 1, bread.objectives.first.progress
    assert_equal [:change], @heard.map(&:first).uniq, "what was saved had happened"
  end

  def test_a_saved_stage_the_file_no_longer_has_is_left_out
    without_file("errand")
    Quests.replace("bread" => { "stage" => "oven", "progress" => [1] }, "errand" => { stage: "asked" })
    assert_nil bread.stage
    assert_equal "asked", Quests.stage(:errand), "a symbol key does as well"
  end

  def test_the_events_are_named
    error = assert_raises(ArgumentError) { Quests.on(:done) { nil } }
    assert error.message.include?(":done")
    assert_raises(ArgumentError) { Quests.collected(:flour, 0) }
  end
end
