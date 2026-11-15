# Conversations and GameVariables over the network (network_dialogues.rb), with this machine
# pretending to be the host or a client: what it sends is written down instead, and what arrives is
# handed to it as the engine would. (Two real machines talk in tests/system_pairs.)
module Network
  class << self
    attr_accessor :pretend_online, :pretend_host, :pretend_id
    attr_reader :sent

    def online? = @pretend_online
    def host? = @pretend_host
    def id = @pretend_id

    def __deliver(name, data, to)
      (@sent ||= []) << [name, data, to]
      true
    end

    def forget_sent = @sent = []
  end
end

# A hero or someone to talk to, with what the network knows it by.
class NetworkedTalker
  Sync = Struct.new(:owner, :key, :game_object) do
    def mine? = !Network.online? || owner == Network.id
  end

  attr_reader :name, :sync

  def initialize(name, owner: nil)
    @name = name
    @sync = owner && Sync.new(owner, "#{name}##{owner}.1", self)
  end

  def get_component(klass) = klass == NetworkSync ? @sync : nil
end

class DialogueNetworkTest < RedGemTest::Case
  GUEST = 2
  STRANGER = 3
  GOLD = 7
  # A guard who lets a player in for a password, and once it was given, says so.
  GATE = {
    "start" => "ask",
    "nodes" => {
      "ask" => { "text" => "gate.ask", "effects" => [{ "add" => "asked", "by" => 1 }], "choices" => [
        { "text" => "gate.password", "if" => [{ "var" => "knows_password", "is" => true }],
          "effects" => [{ "set" => "gate_open", "to" => true }], "next" => "open" },
        { "text" => "gate.leave", "end" => true }
      ] },
      "open" => { "text" => "gate.open", "end" => true }
    }
  }.freeze

  def setup
    as(:host)
    Dialogue.__scene_reset
    GameVariables.replace({})
    Network.forget_sent
    Dialogue.instance_variable_get(:@scripts)["gate"] = Dialogue::Script.new("gate", GATE)
    @guest_hero = NetworkedTalker.new("Hero", owner: GUEST)
    @guard = NetworkedTalker.new("Guard")
    Network.instance_variable_set(:@synced, { @guest_hero.sync.key => @guest_hero.sync })
  end

  def as(role)
    Network.pretend_online = role != :offline
    Network.pretend_host = role == :host
    Network.pretend_id = role == :host ? Network::HOST : GUEST
  end

  def receive(name, data, from) = Network.__event(:message, from, name, data)

  def sent(name) = Network.sent.select { |message| message[0] == name }

  def test_the_host_starts_a_guest_s_conversation_on_the_guest_s_machine
    GameVariables[:knows_password] = true
    Network.forget_sent
    assert Dialogue.start("gate", actor: @guest_hero, speaker: @guard)
    assert_nil Dialogue.current, "not the host's player's"
    assert_equal [[Network::TALK, ["gate", "Hero#2.1", "Guard"], GUEST]], sent(Network::TALK)
    assert_equal "ask", Dialogue.__mirror_of(GUEST).node_id
    assert_equal 1, GameVariables[:asked], "the effects happen on the host"
  end

  def test_the_host_takes_the_guest_s_steps_and_their_effects_happen_there
    GameVariables[:knows_password] = true
    Dialogue.start("gate", actor: @guest_hero, speaker: @guard)
    Network.forget_sent
    receive(Network::STEP, ["gate", "ask", 0, "open"], GUEST)
    assert_equal true, GameVariables[:gate_open]
    assert_equal [], sent(Network::TALK_AT), "the guest is where the host is"
    assert_equal [[Network::VARIABLE, ["gate_open", true], :all]], sent(Network::VARIABLE)
  end

  def test_a_choice_the_host_does_not_offer_is_not_taken
    Dialogue.start("gate", actor: @guest_hero, speaker: @guard)
    Network.forget_sent
    receive(Network::STEP, ["gate", "ask", 0, "open"], GUEST)
    assert_nil GameVariables[:gate_open]
    assert_equal [[Network::TALK_AT, %w[gate ask], GUEST]], sent(Network::TALK_AT), "the guest is put back"
  end

  def test_steps_from_elsewhere_or_from_another_place_are_put_right
    Dialogue.start("gate", actor: @guest_hero, speaker: @guard)
    Network.forget_sent
    receive(Network::STEP, ["gate", "ask", 1, nil], STRANGER)
    receive(Network::STEP, ["other", "ask", 1, nil], GUEST)
    assert_equal [], Network.sent, "nobody else's, and not this dialogue: nothing to say"
    receive(Network::STEP, ["gate", "open", nil, nil], GUEST)
    assert_equal [[Network::TALK_AT, %w[gate ask], GUEST]], sent(Network::TALK_AT)
    assert_equal "ask", Dialogue.__mirror_of(GUEST).node_id
  end

  def test_the_host_forgets_a_conversation_that_ended
    Dialogue.start("gate", actor: @guest_hero, speaker: @guard)
    receive(Network::STEP, ["gate", "ask", 1, nil], GUEST)
    assert_nil Dialogue.__mirror_of(GUEST)
  end

  def test_the_host_forgets_a_conversation_that_was_left
    Dialogue.start("gate", actor: @guest_hero, speaker: @guard)
    receive(Network::TALK_LEFT, ["gate"], GUEST)
    assert_nil Dialogue.__mirror_of(GUEST)
  end

  def test_the_host_forgets_the_conversation_of_a_guest_that_left
    Dialogue.start("gate", actor: @guest_hero, speaker: @guard)
    Network.__event(:left, GUEST, "", nil)
    assert_nil Dialogue.__mirror_of(GUEST)
  end

  def test_a_guest_has_the_conversation_the_host_starts
    as(:guest)
    receive(Network::TALK, ["gate", "Hero#2.1", "Guard"], Network::HOST)
    conversation = Dialogue.current
    refute conversation.deciding?
    assert @guest_hero.equal?(conversation.actor)
    assert_nil GameVariables[:asked], "the effects are the host's"
  end

  def test_a_guest_says_each_step_and_its_effects_wait_for_the_host
    as(:guest)
    GameVariables.__told("knows_password", true)
    receive(Network::TALK, ["gate", "Hero#2.1", nil], Network::HOST)
    Dialogue.current.choose(0)
    assert_nil GameVariables[:gate_open]
    assert_equal [[Network::STEP, ["gate", "ask", 0, "open"], :host]], Network.sent
  end

  def test_a_guest_that_leaves_says_so
    as(:guest)
    receive(Network::TALK, ["gate", "Hero#2.1", nil], Network::HOST)
    Dialogue.stop
    assert_equal [[Network::TALK_LEFT, ["gate"], :host]], Network.sent
  end

  def test_a_guest_goes_where_the_host_says
    as(:guest)
    receive(Network::TALK, ["gate", "Hero#2.1", nil], Network::HOST)
    receive(Network::TALK_AT, %w[gate open], Network::HOST)
    assert_equal "open", Dialogue.current.node_id
    receive(Network::TALK_AT, ["gate", nil], STRANGER)
    assert_equal "open", Dialogue.current.node_id, "only the host says"
    receive(Network::TALK_AT, ["gate", nil], Network::HOST)
    assert_nil Dialogue.current
  end

  def test_only_the_host_starts_conversations
    as(:guest)
    assert_raises(ArgumentError) { Dialogue.start("gate", actor: @guest_hero) }
    receive(Network::TALK, ["gate", "Hero#2.1", nil], STRANGER)
    assert_nil Dialogue.current
  end

  def test_the_game_going_ends_a_guest_s_conversation
    as(:guest)
    receive(Network::TALK, ["gate", "Hero#2.1", nil], Network::HOST)
    Dialogue.__network_gone
    assert_nil Dialogue.current
  end

  def test_the_host_s_own_player_talks_here
    hero = NetworkedTalker.new("Host", owner: Network::HOST)
    conversation = Dialogue.start("gate", actor: hero)
    assert conversation.equal?(Dialogue.current)
    assert conversation.deciding?
  end

  def test_variables_come_from_the_host
    as(:guest)
    assert_raises(ArgumentError) { GameVariables[:gold] = GOLD }
    receive(Network::VARIABLE, ["gold", GOLD], STRANGER)
    assert_nil GameVariables[:gold]
    receive(Network::VARIABLE, ["gold", GOLD], Network::HOST)
    assert_equal GOLD, GameVariables[:gold]
    receive(Network::VARIABLES, [["met", true]], Network::HOST)
    assert_equal({ "met" => true }, GameVariables.to_h)
  end

  def test_a_newcomer_hears_of_every_variable
    GameVariables[:gold] = GOLD
    Network.forget_sent
    Network.__event(:joined, GUEST, "", nil)
    assert_equal [[Network::VARIABLES, [["gold", GOLD]], GUEST]], sent(Network::VARIABLES)
  end

  def test_a_step_is_a_dialogue_a_node_a_choice_and_a_node
    said = Network::Said
    assert said.step?(["gate", "ask", 0, "open"])
    assert said.step?(["gate", "ask", nil, nil])
  end

  def test_what_does_not_fit_is_ignored
    said = Network::Said
    refute said.step?(["gate", "ask", -1, nil])
    refute said.step?(["gate", "", 0, nil])
    refute said.step?(["gate", "x" * (said::MAX_NODE_SIZE + 1), 0, nil])
    refute said.talk?(["gate", nil, nil])
    refute said.variables?([["gold", [1]]])
    refute said.variables?({ "gold" => 1 })
  end
end

# What the host and a guest do when they did not see the same thing.
class DialogueNetworkRecoveryTest < DialogueNetworkTest
  def test_variables_put_back_whole_reach_everyone
    Network.forget_sent
    GameVariables.replace(gold: GOLD)
    assert_equal [[Network::VARIABLES, [["gold", GOLD]], :all]], sent(Network::VARIABLES)
    assert Network::Said.variables?(sent(Network::VARIABLES).first[1])
  end

  def test_the_host_puts_back_a_conversation_that_ended_too_soon_here
    as(:guest)
    receive(Network::TALK, ["gate", "Hero#2.1", nil], Network::HOST)
    Dialogue.current.choose(1)
    assert_nil Dialogue.current
    receive(Network::TALK_AT, %w[gate open], Network::HOST)
    assert_equal "open", Dialogue.current&.node_id
  end

  def test_a_dialogue_a_guest_cannot_read_is_left
    as(:guest)
    receive(Network::TALK, ["nowhere", "Hero#2.1", nil], Network::HOST)
    assert_nil Dialogue.current
    assert_equal [[Network::TALK_LEFT, ["nowhere"], :host]], sent(Network::TALK_LEFT)
  end

  def test_a_speaker_the_guest_could_not_find_goes_as_nobody
    Dialogue.start("gate", actor: @guest_hero, speaker: NetworkedTalker.new("x" * (Network::Said::MAX_KEY_SIZE + 1)))
    assert_equal [[Network::TALK, ["gate", "Hero#2.1", nil], GUEST]], sent(Network::TALK)
  end
end
