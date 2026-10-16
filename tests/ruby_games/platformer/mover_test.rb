# The platformer's own classes, loaded by name from its scripts (redgem-test --ruby ... --game games/platformer).
class MoverTest < RedGemTest::Case
  def test_a_platform_moves_along_the_blender_axis_it_is_given
    assert_vector Vector3.blender(0, 0, 1), Mover::BLENDER_AXES["z"]
    assert_equal %w[x y z], Mover::BLENDER_AXES.keys.sort
  end

  def test_a_platform_moves_three_meters_unless_told_otherwise
    assert_in_delta 3.0, Mover::DEFAULT_DISTANCE
    assert Mover < Behaviour
  end
end
