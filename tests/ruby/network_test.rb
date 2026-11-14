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
  # Not a change of level as the host orders one.
  WRONG_LEVELS = [
    nil, [], [1], %w[1 level2], [1.5, "level2"], [1, ""], [1, :level2], [1, "level2", 3],
    [1, "l" * (Network::Said::MAX_LEVEL_NAME_SIZE + 1)]
  ].freeze
  # An id past what the engine keeps one in: cut down to fit, it would be everyone's (0).
  NO_MACHINES_ID = 2**32

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

  def test_a_change_of_level_comes_as_its_number_and_the_levels_name
    assert Said.level?([1, "level2"])
    WRONG_LEVELS.each { |data| refute Said.level?(data), "#{data.inspect} passed for a change of level" }
  end

  def test_offline_no_level_change_holds_anything_back
    refute Network.changing_level?
    refute Network::Level.holds?(Network::SPAWN)
    assert_in_delta Network::LEVEL_TIMEOUT, Network.level_timeout
  end

  def test_a_pose_may_carry_an_animation_within_what_one_can_be
    assert Network::Said.placement?(ANIMATED)
    assert Network::Said.placement?(["Hero", POSITION, POSITION, [0.0, 0.0, nil, nil]])
    WRONG_ANIMATIONS.each do |animation|
      refute Network::Said.placement?(["Hero", POSITION, POSITION, animation]), animation.inspect
    end
  end

  def test_a_message_to_an_id_no_machine_can_have_is_refused
    error = assert_raises(ArgumentError) { Network.emit(:hit, nil, to: NO_MACHINES_ID) }
    assert_equal "no machine has that id", error.message
  end
end
