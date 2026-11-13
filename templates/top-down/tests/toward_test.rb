# Unit tests: plain Ruby with no game running, for code that needs none. Player is the game's
# own (scripts/player.rb): it loads by name, as it does in the game.
class TowardTest < RedGemTest::Case
  HERE = Vector3.new(0, 1, 0)       # the hero's middle, a meter above the ground
  EAST = Vector3.new(4, 0, 0)       # a spot on the ground, to its right
  NEARBY = Vector3.new(0.1, 0, 0.1) # a spot closer than Player::ARRIVED
  EASTWARD = Vector3.new(1, 0, 0)   # the way east, a meter long
  STEP = 0.1                        # meters the hero goes in a frame
  LONG_STEP = 8.0                   # meters it would go in a frame, on a very slow machine
  LAST_STEP_SHARE = 0.5             # of a long step, what is left to the spot

  def test_the_way_to_a_spot_goes_along_the_ground
    assert_vector EASTWARD, Player.toward(HERE, EAST, STEP)
  end

  def test_the_last_step_stops_on_the_spot
    assert_vector EASTWARD * LAST_STEP_SHARE, Player.toward(HERE, EAST, LONG_STEP)
  end

  def test_there_is_no_way_to_go_once_there
    assert_equal Vector3.zero, Player.toward(HERE, NEARBY, STEP)
  end
end
