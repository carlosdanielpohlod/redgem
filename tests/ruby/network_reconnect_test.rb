# Reconnecting as the prelude sees it, outside any game: the window a host holds places for, and
# what the game's handlers hear. The machines themselves are tested in
# tests/integration/network_reconnect_test.cpp.
class NetworkReconnectTest < RedGemTest::Case
  DROPPED = 2
  WINDOW = 20.0
  TIMED_OUT = "the game could not be reached again in time".freeze
  REMOVED_KEY = "ship#2.5".freeze
  OTHER_KEY = "ship#2.6".freeze
  TEMPLATE = "ship".freeze

  def setup
    Network.reconnect_window = 0
    Network.__scene_reset
  end

  def test_the_window_is_kept_and_has_its_limits
    Network.reconnect_window = WINDOW
    assert_in_delta WINDOW, Network.reconnect_window
    Network.reconnect_window = Network::MOST_RECONNECT_WINDOW
    assert_in_delta Network::MOST_RECONNECT_WINDOW, Network.reconnect_window
    assert_raises(ArgumentError) { Network.reconnect_window = -1 }
    assert_raises(ArgumentError) { Network.reconnect_window = Network::MOST_RECONNECT_WINDOW + 1 }
  end

  def test_a_machine_in_no_game_is_not_reconnecting
    refute Network.reconnecting?
  end

  def test_the_handlers_hear_who_dropped_and_who_is_back
    heard = []
    Network.on_reconnecting { |peer| heard << [:reconnecting, peer] }
    Network.on_reconnected { |peer| heard << [:reconnected, peer] }
    Network.__event(:reconnecting, DROPPED, "", nil)
    Network.__event(:reconnected, DROPPED, "", nil)
    assert_equal [[:reconnecting, DROPPED], [:reconnected, DROPPED]], heard
  end

  def test_a_machine_that_could_not_get_back_hears_why
    reasons = []
    Network.on_lost { |reason| reasons << reason }
    Network.__event(:lost, DROPPED, TIMED_OUT, nil)
    assert_equal [TIMED_OUT], reasons
  end

  def test_an_object_removed_while_away_stays_gone_and_only_that_one
    Network.instance_variable_set(:@despawned_away, [REMOVED_KEY])
    assert Network.__send__(:removed_away?, [REMOVED_KEY, TEMPLATE, DROPPED])
    refute Network.__send__(:removed_away?, [OTHER_KEY, TEMPLATE, DROPPED])
    refute Network.__send__(:removed_away?, REMOVED_KEY)
  end

  def test_what_went_while_away_is_forgotten_with_the_game
    Network.instance_variable_set(:@despawned_away, [REMOVED_KEY])
    Network.__event(:lost, DROPPED, TIMED_OUT, nil)
    assert_equal [], Network.instance_variable_get(:@despawned_away)
  end
end
