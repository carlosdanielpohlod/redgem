class Vector3Test < RedGemTest::Case
  def test_arithmetic
    a = Vector3.new(1, 2, 3)
    b = Vector3.new(4, 5, 6)
    assert_equal Vector3.new(5, 7, 9), a + b
    assert_equal Vector3.new(-3, -3, -3), a - b
    assert_equal Vector3.new(2, 4, 6), a * 2
    assert_equal Vector3.new(0.5, 1, 1.5), a / 2
    assert_equal Vector3.new(-1, -2, -3), -a
  end

  def test_dot_and_cross
    assert_in_delta 32.0, Vector3.new(1, 2, 3).dot(Vector3.new(4, 5, 6))
    assert_equal Vector3.forward * -1, Vector3.right.cross(Vector3.up)
  end

  def test_length_and_normalized
    v = Vector3.new(3, 0, 4)
    assert_in_delta 5.0, v.length
    assert_in_delta 1.0, v.normalized.length
    assert_equal Vector3.zero, Vector3.zero.normalized
  end

  def test_helpers
    v = Vector3.new(1, 2, 3)
    assert_equal Vector3.new(1, 0, 3), v.flat
    assert_equal Vector3.new(1, 9, 3), v.with(y: 9)
    assert_vector Vector3.new(0.5, 1, 1.5), Vector3.zero.lerp(v, 0.5)
    assert_in_delta 5.0, Vector3.zero.distance_to(Vector3.new(0, 0, 5))
    assert_equal [1.0, 2.0, 3.0], v.to_a
  end

  def test_constants_follow_gltf_convention
    assert_equal Vector3.new(0, 0, -1), Vector3.forward
    assert_equal Vector3.new(0, 1, 0), Vector3.up
  end

  def test_blender_coordinates_convert_to_y_up
    assert_equal Vector3.new(1, 3, -2), Vector3.blender(1, 2, 3)
    assert_equal Vector3.forward, Vector3.blender(0, 1, 0)
  end

  def test_formatting
    assert_equal "(1.000, 2.000, 3.000)", Vector3.new(1, 2, 3).to_s
  end
end
