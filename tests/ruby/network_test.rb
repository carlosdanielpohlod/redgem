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
  ATTACK = ["Attack", 0.2, 1.0, false, true].freeze
  ANIMATED = ["Hero", POSITION, POSITION, [0.1, 0.5, ATTACK, nil]].freeze
  # What a peer could put in a pose's animation that the binding would refuse, every frame.
  WRONG_ANIMATIONS = [
    [-0.1, 0.5, ATTACK, nil], [0.1, 1.5, ATTACK, nil], [0.1, -0.5, nil, nil],
    [0.1, 0.5, ["", 0.2, 1.0, true, true], nil],
    [0.1, 0.5, [:Attack, 0.2, 1.0, true, true], nil], [0.1, 0.5, ["Attack", 0.2, 1.0, 1, true], nil],
    [0.1, 0.5, ["x" * (Network::Said::MAX_CLIP_NAME_SIZE + 1), 0.0, 1.0, true, true], nil], [0.1, 0.5, ATTACK]
  ].freeze

  Said = Network::Said
  Resends = Network::Resends
  ASKER = 7             # a machine asking the host what is spawned
  OTHER_ASKER = 8
  NOON = 100.0          # seconds of game time
  ONE_MORE = Resends::SECONDS_FOR_ONE   # what earns a machine another answer
  LONG_AFTER = 3600.0

  def setup
    [ASKER, OTHER_ASKER].each { |peer| Resends.forget(peer) }
  end

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
    assert Said.vector?(POSITION)
    assert Said.vector?([1, 2, 3])
    refute Said.vector?([1.0, 2.0])
    refute Said.vector?([1.0, 2.0, "3"])
    refute Said.vector?("1, 2, 3")
  end

  def test_a_vector_is_somewhere_an_object_can_be
    assert Said.vector?([FAR, -FAR, 0])
    refute Said.vector?([FAR * 2, 0, 0])
    refute Said.vector?([0, -FAR * 2, 0])
    refute Said.vector?([Float::INFINITY, 0, 0])
    refute Said.vector?([0, Float::NAN, 0])
  end

  def test_peer_ids_go_from_the_host_to_a_full_game
    assert Said.peer_id?(Network::HOST)
    assert Said.peer_id?(7)
    assert Said.peer_id?(Network::Said::MAX_PEERS)
    refute Said.peer_id?(Network::Said::MAX_PEERS + 1)
    refute Said.peer_id?(Network::LOCAL)
    refute Said.peer_id?("2")
    refute Said.peer_id?(2.0)
  end

  def test_a_spawn_record_must_be_well_formed
    assert Said.record?(RECORD)
    assert Said.record?(["Ball#2.1", "Ball", 2, POSITION, POSITION])
    refute Said.record?(nil)
    refute Said.record?(RECORD.first(4))
    refute Said.record?([:key, "Ball", 2, nil, nil])
    refute Said.record?(["Ball#2.1", "Ball", 0, nil, nil])
    refute Said.record?(["Ball#2.1", "Ball", 2, [1.0], nil])
    refute Said.record?(["Ball#2.1", "Ball", 2, nil, [FAR * 2, 0, 0]])
  end

  def test_a_spawned_objects_key_is_its_makers_count_of_that_template
    assert Said.key_of?("Ball#2.1", "Ball", 2)
    assert Said.key_of?("Ball#2.#{MOST_DIGITS}", "Ball", 2)
    WRONG_KEYS.each { |key| refute Said.key_of?(key, "Ball", 2), "#{key} passed for a key of machine 2" }
    refute Said.key_of?("#{LONG_NAME}#2.1", LONG_NAME, 2)
  end

  def test_where_an_object_is_comes_as_a_key_and_two_vectors
    assert Said.placement?(["Platform", POSITION, POSITION])
    WRONG_PLACEMENTS.each { |data| refute Said.placement?(data), "#{data.inspect} passed for a placement" }
  end

  def test_a_machine_asking_what_is_spawned_is_answered_a_few_times_in_a_row
    Resends::SAVED_UP.times { assert Resends.answer?(ASKER, NOON) }
    refute Resends.answer?(ASKER, NOON), "one more than it had saved up"
    assert Resends.answer?(OTHER_ASKER, NOON), "each machine has answers of its own"
  end

  def test_a_machine_that_used_its_answers_up_earns_one_every_while
    Resends::SAVED_UP.times { Resends.answer?(ASKER, NOON) }
    refute Resends.answer?(ASKER, NOON + ONE_MORE * HALF)
    assert Resends.answer?(ASKER, NOON + ONE_MORE), "asking too early did not put the next answer off"
    refute Resends.answer?(ASKER, NOON + ONE_MORE)
  end

  def test_answers_are_only_saved_up_so_far
    Resends.answer?(ASKER, NOON)
    Resends::SAVED_UP.times { assert Resends.answer?(ASKER, LONG_AFTER) }
    refute Resends.answer?(ASKER, LONG_AFTER)
  end

  def test_whoever_comes_next_under_an_id_starts_with_all_its_answers
    Resends::SAVED_UP.times { Resends.answer?(ASKER, NOON) }
    Resends.forget(ASKER)
    assert Resends.answer?(ASKER, NOON)
  end

  def test_a_pose_may_carry_an_animation_within_what_one_can_be
    assert Network::Said.placement?(ANIMATED)
    assert Network::Said.placement?(["Hero", POSITION, POSITION, [0.0, 0.0, nil, nil]])
    WRONG_ANIMATIONS.each do |animation|
      refute Network::Said.placement?(["Hero", POSITION, POSITION, animation]), animation.inspect
    end
  end
end
