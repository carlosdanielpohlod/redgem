# What a game asks the network about dedicated servers, outside any game: the server's own side
# and its players are tested with servers running (tests/integration/dedicated_server_test.cpp,
# tests/system_servers).
class NetworkServerTest < RedGemTest::Case
  NOBODY_THERE = "127.0.0.1".freeze
  SERVER_PORT = 47_823   # not the usual one: a game may be open on this machine
  CODE = "K7QM3P".freeze

  # Each test starts in no game, with no relay set.
  def setup
    Network.leave
    Network.relay = nil
  end

  def test_a_machine_in_no_game_is_no_server_and_plays_with_nobody
    refute Network.server?
    refute Network.host?
    assert_equal [], Network.players
  end

  def test_a_room_is_joined_on_the_server_at_the_address_given
    assert Network.join(NOBODY_THERE, port: SERVER_PORT, room: CODE.downcase)
    assert Network.connecting?
    assert_equal SERVER_PORT, Network.port
  end

  def test_a_room_with_no_address_is_joined_on_the_relay_as_before
    Network.relay = NOBODY_THERE
    assert Network.join(room: CODE)
    assert_equal Network::DEFAULT_RELAY_PORT, Network.port
  end

  def test_a_room_with_no_address_and_no_relay_says_where_to_set_one
    error = assert_raises(ArgumentError) { Network.join(room: CODE) }
    assert error.message.include?("Network.relay"), error.message
  end
end
