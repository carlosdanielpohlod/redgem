# Unit tests: plain Ruby with no game running. Spawns is the game's own (scripts/spawns.rb): it
# loads by name, as it does in the game.
class SpawnsTest < RedGemTest::Case
  POINTS = 2
  HOST = 1          # the host's id (and the one playing alone)
  SECOND = 2        # the next machine to join
  THIRD = 3

  def test_the_host_and_the_next_player_get_points_of_their_own
    assert_equal 0, Spawns.index(HOST, POINTS)
    assert_equal 1, Spawns.index(SECOND, POINTS)
  end

  def test_more_players_than_points_start_again_from_the_first
    assert_equal 0, Spawns.index(THIRD, POINTS)
  end
end
