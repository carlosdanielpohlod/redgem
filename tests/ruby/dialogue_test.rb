# Conversations as one player has them, offline (dialogue.rb): where a dialogue starts, which
# choices it offers, what its effects change, and who hears of it. The dialogue is given as the
# engine reads its file (string keys); the people in it are stand-ins.

# What a scene object answers that a conversation asks.
class Talker
  Place = Struct.new(:position)

  attr_reader :name, :behaviours

  def initialize(name)
    @name = name
    @behaviours = []
  end

  def props = {}
  def alive? = true
  def children = []
  def animation = Animator.new(self)
  def transform = Place.new(Vector3.zero)
  def get_component(klass) = @behaviours.find { |behaviour| behaviour.is_a?(klass) }
  def __add_behaviour(behaviour) = @behaviours << behaviour

  def add_component(klass, **settings)
    behaviour = klass.new
    behaviour.__configure(settings)
    behaviour.__attach(self)
    behaviour
  end
end

class DialogueTest < RedGemTest::Case
  PRICE = 5
  RICH = 12
  BAKER = {
    "start" => [{ "if" => [{ "var" => "met_baker", "is" => true }], "next" => "again" }, { "next" => "hello" }],
    "nodes" => {
      "hello" => { "speaker" => "baker.name", "text" => "baker.hello", "voice" => "baker/hello",
                   "effects" => [{ "set" => "met_baker", "to" => true }], "next" => "offer" },
      "again" => { "text" => "baker.again", "next" => "offer" },
      "offer" => { "text" => "baker.offer", "choices" => [
        { "text" => "baker.buy", "if" => [{ "var" => "gold", "at_least" => PRICE }],
          "effects" => [{ "add" => "gold", "by" => -PRICE }, { "quest" => "bread", "to" => "done" },
                        { "event" => "sold", "data" => PRICE }],
          "next" => "thanks" },
        { "text" => "baker.bye", "end" => true }
      ] },
      "thanks" => { "text" => "baker.thanks", "end" => true }
    }
  }.freeze

  def setup
    Dialogue.__scene_reset
    GameVariables.replace({})
    GameVariables.__scene_reset
    Dialogue.quests = nil
    @hero = Talker.new("Hero")
    @baker = Talker.new("Baker")
    @script = Dialogue::Script.new("baker", BAKER)
    Dialogue.instance_variable_get(:@scripts)["baker"] = @script
  end

  def talk = Dialogue.start("baker", actor: @hero, speaker: @baker)

  def test_a_conversation_starts_where_the_dialogue_says
    conversation = talk
    assert conversation.equal?(Dialogue.current)
    assert_equal "hello", conversation.node_id
    assert_equal "baker.hello", conversation.text, "a key with no text shows as itself"
    assert_equal "baker.name", conversation.speaker_name
    assert_equal "baker/hello", conversation.voice
    assert @baker.equal?(conversation.speaker)
  end

  def test_a_voice_with_no_sound_plays_nothing_and_the_line_waits
    conversation = talk
    refute conversation.speaking?
    conversation.__update
    assert_equal "hello", conversation.node_id
  end

  def test_a_mouth_opens_as_far_as_its_voice_is_loud
    assert_in_delta 0.5, VoiceMouth.openness_at(0.1, 0.2), 1e-6
    assert_in_delta VoiceMouth::OPEN, VoiceMouth.openness_at(0.9, 0.2), 1e-6
    assert_in_delta VoiceMouth::SHUT, VoiceMouth.openness_at(0.0, 0.2), 1e-6
  end

  def test_a_mouth_needs_a_full_loudness_above_zero
    mouth = @baker.add_component(VoiceMouth, morph: "MouthOpen", full: 0)
    mouth.follow(1)
    assert_raises(ArgumentError) { mouth.update(0.1) }
  end

  def test_a_mouth_that_follows_nothing_stays_shut
    mouth = @baker.add_component(VoiceMouth, morph: "MouthOpen")
    mouth.update(0.1)
    assert_in_delta VoiceMouth::SHUT, mouth.openness, 1e-6
  end

  def test_the_start_takes_the_first_branch_that_holds
    GameVariables[:met_baker] = true
    assert_equal "again", talk.node_id
  end

  def test_a_node_s_effects_happen_as_it_is_reached
    talk
    assert_equal true, GameVariables[:met_baker]
  end

  def test_continue_goes_on_and_waits_for_a_choice
    conversation = talk
    assert conversation.continue
    assert_equal "offer", conversation.node_id
    refute conversation.continue, "the player must choose"
  end

  def test_every_line_shown_counts_a_step_also_one_shown_again
    conversation = talk
    assert_equal 1, conversation.steps
    conversation.continue
    conversation.__jump("offer")
    assert_equal 3, conversation.steps
  end

  def test_only_the_choices_whose_conditions_hold_are_offered
    conversation = talk.tap(&:continue)
    assert_equal ["baker.bye"], conversation.choices.map(&:key)
    refute conversation.choose(0), "the bread is not offered without gold"
    GameVariables[:gold] = RICH
    assert_equal [0, 1], conversation.choices.map(&:index)
  end

  def test_a_choice_s_effects_happen_and_it_leads_on
    GameVariables[:gold] = RICH
    conversation = talk.tap(&:continue)
    assert conversation.choose(conversation.choices.first)
    assert_equal "thanks", conversation.node_id
    assert_equal RICH - PRICE, GameVariables[:gold]
    assert_equal "done", Dialogue.quests.stage("bread")
  end

  def test_the_game_hears_of_a_dialogue_s_events
    GameVariables[:gold] = RICH
    sold = []
    Dialogue.on(:event) { |name, data, conversation| sold << [name, data, conversation.speaker.name] }
    talk.tap(&:continue).choose(0)
    assert_equal [["sold", PRICE, "Baker"]], sold
  end

  def test_the_end_ends_it_and_the_player_hears_of_every_step
    heard = []
    %i[start line end].each { |event| Dialogue.on(event) { |conversation| heard << [event, conversation.node_id] } }
    conversation = talk.tap(&:continue)
    conversation.choose(1)
    assert conversation.over?
    assert_nil Dialogue.current
    assert_equal [[:start, nil], [:line, "hello"], [:line, "offer"], [:end, nil]], heard
  end

  def test_a_line_whose_choices_are_all_hidden_ends_on_continue
    script = Dialogue::Script.new("gate", { "start" => "a", "nodes" => {
                                    "a" => { "text" => "t", "choices" => [
                                      { "text" => "c", "if" => [{ "var" => "key", "is" => true }], "end" => true }
                                    ] }
                                  } })
    conversation = Dialogue.start(script, actor: @hero)
    refute conversation.choosing?
    assert conversation.continue
    assert conversation.over?
  end

  def test_another_conversation_or_stop_ends_the_one_there_was
    first = talk
    second = talk
    assert first.over?
    assert second.equal?(Dialogue.current)
    assert Dialogue.stop
    assert second.over?
    refute Dialogue.stop, "nothing left to end"
  end

  # Quests a game keeps its own way: in a hash.
  def quests_in(stages)
    quests = Object.new
    quests.define_singleton_method(:stage) { |name| stages[name] }
    quests.define_singleton_method(:set_stage) { |name, stage, _conversation| stages[name] = stage }
    quests
  end

  def test_a_game_can_keep_quests_its_own_way
    stages = {}
    Dialogue.quests = quests_in(stages)
    GameVariables[:gold] = RICH
    talk.tap(&:continue).choose(0)
    assert_equal({ "bread" => "done" }, stages)
    assert_nil GameVariables["quest.bread"]
  end

  def test_adding_to_what_is_no_number_raises
    conversation = talk
    GameVariables[:gold] = "lots"
    assert_raises(ArgumentError) { Dialogue::Rules.apply([{ "add" => "gold", "by" => 1 }], conversation) }
  end

  def test_talkable_starts_its_dialogue_and_adds_what_is_used
    talkable = @baker.add_component(Talkable, dialogue: "baker", range: 3.0)
    talkable.start
    interactable = @baker.get_component(Interactable)
    assert_equal Talkable::PROMPT, interactable.prompt
    assert_in_delta 3.0, interactable.range
    talkable.on_interact(@hero)
    assert_equal "hello", Dialogue.current.node_id
    assert @hero.equal?(Dialogue.current.actor)
  end

  def test_talkable_without_a_dialogue_says_so
    talkable = @baker.add_component(Talkable)
    error = assert_raises(ArgumentError) { talkable.on_interact(@hero) }
    assert error.message.include?("Baker")
  end

  def test_the_events_are_named
    error = assert_raises(ArgumentError) { Dialogue.on(:said) { nil } }
    assert error.message.include?(":said")
  end
end

# The values every player shares (game_variables.rb).
class GameVariablesTest < RedGemTest::Case
  def setup
    GameVariables.replace({})
    GameVariables.__scene_reset
  end

  def test_a_variable_holds_what_it_was_set_to
    assert_nil GameVariables[:gold]
    GameVariables[:gold] = 3
    assert_equal 3, GameVariables["gold"], "a symbol stands for its name"
    GameVariables[:gold] = nil
    assert_equal({}, GameVariables.to_h, "nil is no value")
  end

  def test_what_a_variable_cannot_be_raises
    assert_raises(ArgumentError) { GameVariables[""] = 1 }
    assert_raises(ArgumentError) { GameVariables["x" * (GameVariables::MAX_NAME_SIZE + 1)] = 1 }
    assert_raises(ArgumentError) { GameVariables[:list] = [1] }
    assert_raises(ArgumentError) { GameVariables[:far] = Float::INFINITY }
    assert_raises(ArgumentError) { GameVariables[:long] = "x" * (GameVariables::MAX_TEXT_SIZE + 1) }
  end

  def test_the_game_hears_of_every_change
    heard = []
    GameVariables.on_change { |name, value| heard << [name, value] }
    GameVariables[:gold] = 1
    GameVariables.replace(gold: 1, met: true)
    GameVariables.replace({})
    assert_equal [["gold", 1], ["met", true], ["gold", nil], ["met", nil]], heard
  end

  def test_they_are_saved_and_put_back_whole
    GameVariables[:gold] = 4
    saved = GameVariables.to_h
    GameVariables[:gold] = 9
    GameVariables.replace(saved)
    assert_equal 4, GameVariables[:gold]
    assert_raises(ArgumentError) { GameVariables.replace(gold: [4]) }
  end

  def test_there_is_room_for_so_many
    GameVariables.replace((1..GameVariables::MAX_COUNT).to_h { |count| ["v#{count}", count] })
    assert_raises(ArgumentError) { GameVariables[:one_more] = 1 }
    GameVariables[:v1] = 0
    assert_equal 0, GameVariables[:v1], "one already there still changes"
  end
end
