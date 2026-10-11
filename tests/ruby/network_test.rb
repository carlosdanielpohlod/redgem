class NetworkTest < RedGemTest::Case
  HALF = 0.5
  POSITION = [1.0, 2.0, 3.0].freeze
  RECORD = ["Ball#2.1", "Ball", 2, POSITION, nil].freeze

  # Asks Network one of the questions it only asks itself.
  def ask(question, *) = Network.__send__(question, *)

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
    assert ask(:vector?, POSITION)
    assert ask(:vector?, [1, 2, 3])
    refute ask(:vector?, [1.0, 2.0])
    refute ask(:vector?, [1.0, 2.0, "3"])
    refute ask(:vector?, "1, 2, 3")
  end

  def test_peer_ids_start_at_the_host
    assert ask(:peer_id?, Network::HOST)
    assert ask(:peer_id?, 7)
    refute ask(:peer_id?, Network::LOCAL)
    refute ask(:peer_id?, "2")
    refute ask(:peer_id?, 2.0)
  end

  def test_a_machine_speaks_for_itself_and_the_host_for_everyone
    assert ask(:speaks_for?, 2, 2)
    assert ask(:speaks_for?, Network::HOST, 2)
    refute ask(:speaks_for?, 3, 2)
  end

  def test_a_spawn_record_must_be_well_formed
    assert ask(:well_formed?, RECORD)
    assert ask(:well_formed?, ["Ball#2.1", "Ball", 2, POSITION, POSITION])
    refute ask(:well_formed?, nil)
    refute ask(:well_formed?, RECORD.first(4))
    refute ask(:well_formed?, [:key, "Ball", 2, nil, nil])
    refute ask(:well_formed?, ["Ball#2.1", "Ball", 0, nil, nil])
    refute ask(:well_formed?, ["Ball#2.1", "Ball", 2, [1.0], nil])
  end

  def test_a_spawn_of_something_the_level_does_not_have_is_refused
    refute ask(:spawn_allowed?, RECORD, 2)
    refute ask(:spawnable?, "Ball")
  end

  def test_nothing_is_spawned_in_an_empty_level
    assert_equal [], ask(:spawned_by, Network::LOCAL)
    assert_equal [], ask(:keys_spawned_by, Network::HOST)
  end

  def test_engine_messages_are_answered_by_methods_that_exist
    answers = Network::ENGINE_MESSAGES.values + Network::REACTIONS.values
    answers.each { |answer| assert Network.respond_to?(answer, true), "Network.#{answer} is missing" }
  end
end
