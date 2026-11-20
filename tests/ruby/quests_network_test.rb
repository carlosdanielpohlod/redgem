# Quests over the network (quests.rb, network_quests.rb), with this machine pretending to be the
# host or a client: what it sends is written down instead, and what arrives is handed to it as the
# engine would. (Two real machines play a quest in tests/system_pairs.)
class QuestsNetworkTest < RedGemTest::Case
  class << Network
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

  GUEST = 2
  SACKS = 2
  FLOUR = {
    "title" => "flour.title", "start" => "collect",
    "stages" => {
      "collect" => { "text" => "flour.collect", "objectives" => [
        { "collect" => "flour", "count" => SACKS, "text" => "flour.sacks" }
      ], "next" => "done" },
      "done" => { "text" => "flour.done", "end" => true }
    }
  }.freeze

  def setup
    as(:offline)
    Quests.__scene_reset
    GameVariables.replace({})
    Quests.instance_variable_get(:@definitions)["flour"] = Quests::Definition.new("flour", FLOUR)
    Network.forget_sent
    @heard = []
    Quests::EVENTS.each { |event| Quests.on(event) { |quest, *_rest| @heard << [event, quest.name] } }
  end

  def as(role)
    Network.pretend_online = role != :offline
    Network.pretend_host = role == :host
    Network.pretend_id = role == :host ? Network::HOST : GUEST
  end

  def receive(name, data, from) = Network.__event(:message, from, name, data)

  def sent(name) = Network.sent.select { |message| message[0] == name }

  def test_a_client_tells_the_host_what_its_player_collected
    Quests.start(:flour)
    as(:client)
    assert Quests.collected(:flour)
    assert_equal [[Network::QUEST_REPORT, ["collect", "flour", nil, 1], :host]], sent(Network::QUEST_REPORT)
    assert_equal 0, Quests[:flour].objectives.first.progress, "the host counts it"
  end

  def test_a_client_does_not_move_quests
    as(:client)
    assert_raises(ArgumentError) { Quests.start(:flour) }
    assert_raises(ArgumentError) { Quests.replace({}) }
    assert_nil Quests.stage(:flour)
  end

  def test_the_host_counts_what_a_client_said_and_tells_everyone
    as(:host)
    Quests.start(:flour)
    Network.forget_sent
    receive(Network::QUEST_REPORT, ["collect", "flour", nil, SACKS], GUEST)
    assert_equal "done", Quests.stage(:flour)
    told = sent(Network::VARIABLE)
    assert told.include?([Network::VARIABLE, ["quest.flour#0", SACKS], :all])
    assert told.include?([Network::VARIABLE, ["quest.flour", "done"], :all])
  end

  def test_the_host_ignores_what_is_not_a_report
    as(:host)
    Quests.start(:flour)
    receive(Network::QUEST_REPORT, ["collect", "flour", nil, 0], GUEST)
    receive(Network::QUEST_REPORT, ["talk_to", "Baker", nil, 1], GUEST)
    receive(Network::QUEST_REPORT, %w[collect flour], GUEST)
    receive(Network::QUEST_REPORT, ["collect", "flour", nil, 1], Network::HOST)
    assert_equal 0, Quests[:flour].objectives.first.progress
  end

  def test_a_client_hears_of_the_quests_as_the_host_moves_them
    as(:client)
    receive(Network::VARIABLE, ["quest.flour", "collect"], Network::HOST)
    receive(Network::VARIABLE, ["quest.flour#0", 1], Network::HOST)
    receive(Network::VARIABLE, ["quest.flour", "done"], Network::HOST)
    assert Quests[:flour].complete?
    assert_equal %i[start stage change objective change stage complete change], @heard.map(&:first)
  end

  def test_a_newcomer_hears_only_that_they_changed
    as(:client)
    receive(Network::VARIABLES, [["quest.flour", "collect"], ["quest.flour#0", 1]], Network::HOST)
    assert_equal 1, Quests[:flour].objectives.first.progress
    assert_equal [:change], @heard.map(&:first).uniq
  end
end
