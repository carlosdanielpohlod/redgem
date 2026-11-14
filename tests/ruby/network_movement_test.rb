# Movement validation as a game turns it on (network_movement.rb): the host's own side is tested
# with machines in a game (tests/integration/movement_validation_test.cpp, multiplayer_test.cpp).
class NetworkMovementTest < RedGemTest::Case
  SPEED = 9.0
  TELEPORT = 4.0
  TOLERANCE = 0.5
  KICK_AFTER = 10
  POSITION = [1.0, 2.0, 3.0].freeze
  FAR = Network::Said::MAX_DISTANCE
  WRONG_CORRECTIONS = [nil, [], ["hero"], [7, POSITION], ["hero", [1.0, 2.0]], ["hero", [FAR * 2, 0, 0]],
                       ["hero", POSITION, POSITION]].freeze
  STRANGER = 9   # a machine that is not the host

  # Each test starts with nothing checked, as a game does.
  def setup
    Network.validate_movement
  end

  def test_nothing_is_checked_until_the_game_says
    rules = Network.movement_rules
    assert_in_delta 0.0, rules[:max_speed]
    assert_in_delta 0.0, rules[:teleport_distance]
    assert_in_delta Network::MOVEMENT_TOLERANCE, rules[:tolerance]
    assert_equal 0, rules[:kick_after]
    refute rules[:undo]
  end

  def test_the_rules_a_game_gives_are_the_hosts
    Network.validate_movement(max_speed: SPEED, teleport_distance: TELEPORT, tolerance: TOLERANCE,
                              kick_after: KICK_AFTER, undo: true)
    rules = Network.movement_rules
    assert_in_delta SPEED, rules[:max_speed]
    assert_in_delta TELEPORT, rules[:teleport_distance]
    assert_in_delta TOLERANCE, rules[:tolerance]
    assert_equal KICK_AFTER, rules[:kick_after]
    assert rules[:undo]
  end

  def test_a_limit_out_of_range_is_refused
    assert_raises(ArgumentError) { Network.validate_movement(max_speed: -1) }
    assert_raises(ArgumentError) { Network.validate_movement(teleport_distance: Float::INFINITY) }
    assert_raises(ArgumentError) { Network.validate_movement(kick_after: -1) }
  end

  def test_nobody_has_violations_outside_a_game
    assert_equal 0, Network.movement_violations(2)
    assert_equal 0, Network.movement_violations(-1)
  end

  def test_a_correction_is_a_key_and_a_place
    assert Network::Said.correction?(["hero", POSITION])
    WRONG_CORRECTIONS.each { |data| refute Network::Said.correction?(data), data.inspect }
  end

  def test_only_the_host_corrects
    assert Network.__send__(:host_correction?, ["hero", POSITION], Network::HOST)
    refute Network.__send__(:host_correction?, ["hero", POSITION], STRANGER)
  end
end
