# Unit tests: plain Ruby with no game running, for code that needs none. Player is the game's
# own (scripts/player.rb): it loads by name, as it does in the game.
class TowardTest < RedGemTest::Case
  HERE = Vector3.new(0, 1, 0)       # the hero's middle, a meter above the ground
  EAST = Vector3.new(4, 0, 0)       # a spot on the ground, to its right
  NEARBY = Vector3.new(0.1, 0, 0.1) # a spot closer than Player::ARRIVED
  EASTWARD = Vector3.new(1, 0, 0)   # the way east, a meter long

  def test_the_way_to_a_spot_goes_along_the_ground
    assert_vector EASTWARD, Player.toward(HERE, EAST)
  end

  def test_there_is_no_way_to_go_once_there
    assert_equal Vector3.zero, Player.toward(HERE, NEARBY)
  end
end
