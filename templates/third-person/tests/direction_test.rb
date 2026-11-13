# Unit tests: plain Ruby with no game running, for code that needs none. Player is the game's
# own (scripts/player.rb): it loads by name, as it does in the game.
class DirectionTest < RedGemTest::Case
  AWAY = Vector3.new(0, 0, -1)   # a camera behind the hero looks along -Z
  LEFT = Vector3.new(-1, 0, 0)   # one on its right side looks along -X
  RIGHT = Vector3.new(1, 0, 0)
  STILL = 0.0                    # an axis let go
  FULL = 1.0                     # an axis pushed all the way

  def test_up_walks_where_the_camera_looks
    assert_vector AWAY, Player.direction(AWAY, STILL, FULL)
    assert_vector LEFT, Player.direction(LEFT, STILL, FULL)
  end

  def test_right_walks_to_the_cameras_right
    assert_vector RIGHT, Player.direction(AWAY, FULL, STILL)
  end

  def test_a_diagonal_is_no_faster_than_a_straight_line
    assert_in_delta FULL, Player.direction(AWAY, FULL, FULL).length
  end
end
