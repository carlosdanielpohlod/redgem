class NetworkTest < RedGemTest::Case
  HALF = 0.5
  POSITION = [1.0, 2.0, 3.0].freeze
  RECORD = ["Ball#2.1", "Ball", 2, POSITION, nil].freeze

  FAR = Network::Said::MAX_DISTANCE
  MOST_DIGITS = "9" * Network::Said::MAX_COUNT_DIGITS
  LONG_NAME = "n" * Network::Said::MAX_KEY_SIZE
  # Not what machine 2 calls a Ball it spawns: another's name for one, a name of the game's own
  # choosing, and counts written as no machine writes them.
  WRONG_KEYS = [
    "Ball#1.1", "Ball#22.1", "Crate#2.1", "hero-1", "Ball", "Ball#2.", "Ball#2.0", "Ball#2.01", "Ball#2.-1",
    "Ball#2.1x", "Ball#2.1.5", "Ball#2. 1", "Ball#2.#{MOST_DIGITS}0", "xBall#2.1"
  ].freeze
  WRONG_PLACEMENTS = [
    nil, "nonsense", [], ["Platform"], ["Platform", POSITION], [7, POSITION, POSITION], ["Platform", "here", POSITION],
    ["Platform", POSITION, nil], ["Platform", [1.0, 2.0], POSITION], ["Platform", POSITION, POSITION, POSITION],
    ["Platform", [Float::INFINITY, 0, 0], POSITION]
  ].freeze

  # Asks Network one of the questions it only asks itself.
  def ask(question, *) = Network.__send__(question, *)

  # Asks whether what another machine said fits.
  def said(question, *) = Network::Said.__send__(question, *)

  # As many spawned objects as asked for, all of one owner.
  def crowd(count, owner:) = (1..count).to_h { |number| ["Ball##{owner}.#{number}", { owner: owner }] }

  def test_toward_goes_part_of_the_way
    assert_in_delta 15.0, NetworkSync.toward(10.0, 20.0, HALF)
    assert_in_delta 20.0, NetworkSync.toward(10.0, 20.0, 1.0)
    assert_in_delta 10.0, NetworkSync.toward(10.0, 20.0, 0.0)
  end

  def test_toward_takes_the_short_way_round
    assert_in_delta 360.0, NetworkSync.toward(350.0, 10.0, HALF)
    assert_in_delta 0.0, NetworkSync.toward(10.0, 350.0, HALF)
    assert_in_delta(-175.0, NetworkSync.toward(-170.0, 180.0, HALF))
  end

  def test_a_vector_travels_as_three_numbers
    assert said(:vector?, POSITION)
    assert said(:vector?, [1, 2, 3])
    refute said(:vector?, [1.0, 2.0])
    refute said(:vector?, [1.0, 2.0, "3"])
    refute said(:vector?, "1, 2, 3")
  end

  def test_a_vector_is_somewhere_an_object_can_be
    assert said(:vector?, [FAR, -FAR, 0])
    refute said(:vector?, [FAR * 2, 0, 0])
    refute said(:vector?, [0, -FAR * 2, 0])
    refute said(:vector?, [Float::INFINITY, 0, 0])
    refute said(:vector?, [0, Float::NAN, 0])
  end

  def test_peer_ids_go_from_the_host_to_a_full_game
    assert said(:peer_id?, Network::HOST)
    assert said(:peer_id?, 7)
    assert said(:peer_id?, Network::Said::MAX_PEERS)
    refute said(:peer_id?, Network::Said::MAX_PEERS + 1)
    refute said(:peer_id?, Network::LOCAL)
    refute said(:peer_id?, "2")
    refute said(:peer_id?, 2.0)
  end

  def test_a_machine_speaks_for_itself
    assert ask(:speaks_for?, 2, 2)
    refute ask(:speaks_for?, 3, 2)
  end

  def test_a_spawn_record_must_be_well_formed
    assert said(:record?, RECORD)
    assert said(:record?, ["Ball#2.1", "Ball", 2, POSITION, POSITION])
    refute said(:record?, nil)
    refute said(:record?, RECORD.first(4))
    refute said(:record?, [:key, "Ball", 2, nil, nil])
    refute said(:record?, ["Ball#2.1", "Ball", 0, nil, nil])
    refute said(:record?, ["Ball#2.1", "Ball", 2, [1.0], nil])
    refute said(:record?, ["Ball#2.1", "Ball", 2, nil, [FAR * 2, 0, 0]])
  end

  def test_a_spawned_objects_key_is_its_makers_count_of_that_template
    assert said(:key_of?, "Ball#2.1", "Ball", 2)
    assert said(:key_of?, "Ball#2.#{MOST_DIGITS}", "Ball", 2)
    WRONG_KEYS.each { |key| refute said(:key_of?, key, "Ball", 2), "#{key} passed for a key of machine 2" }
    refute said(:key_of?, "#{LONG_NAME}#2.1", LONG_NAME, 2)
  end

  def test_where_an_object_is_comes_as_a_key_and_two_vectors
    assert said(:placement?, ["Platform", POSITION, POSITION])
    WRONG_PLACEMENTS.each { |data| refute said(:placement?, data), "#{data.inspect} passed for a placement" }
  end

  def test_a_spawn_of_something_the_level_does_not_have_is_refused
    refute ask(:spawn_allowed?, RECORD, 2)
    refute ask(:spawnable?, "Ball")
  end

  def test_nothing_is_spawned_in_an_empty_level
    assert_equal [], ask(:spawned_by, Network::LOCAL)
    assert_equal [], ask(:keys_spawned_by, Network::HOST)
  end

  def test_a_game_holds_only_so_many_spawned_objects
    assert ask(:room_for?, 2)
    Network.instance_variable_set(:@spawned, crowd(Network::MAX_SPAWNS_PER_PEER, owner: 2))
    refute ask(:room_for?, 2)
    assert ask(:room_for?, 3)
    Network.instance_variable_set(:@spawned, crowd(Network::MAX_SPAWNS, owner: 2))
    refute ask(:room_for?, 3)
  ensure
    Network.instance_variable_set(:@spawned, {})
  end

  def test_engine_messages_are_answered_by_methods_that_exist
    answers = Network::ENGINE_MESSAGES.values + Network::REACTIONS.values
    answers.each { |answer| assert Network.respond_to?(answer, true), "Network.#{answer} is missing" }
  end
end
