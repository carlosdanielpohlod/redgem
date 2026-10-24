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

  def test_clamped_shortens_only_what_is_too_long
    assert_in_delta 1.0, Vector3.new(1, 0, 1).clamped.length
    assert_equal Vector3.new(0.5, 0, 0), Vector3.new(0.5, 0, 0).clamped
    assert_in_delta 2.0, Vector3.new(0, 8, 0).clamped(2.0).length
    assert_equal Vector3.zero, Vector3.zero.clamped
  end

  def test_helpers
    v = Vector3.new(1, 2, 3)
    assert_equal Vector3.new(1, 0, 3), v.flat
    assert_equal Vector3.new(1, 9, 3), v.with(y: 9)
    assert_vector Vector3.new(0.5, 1, 1.5), Vector3.zero.lerp(v, 0.5)
    assert_in_delta 5.0, Vector3.zero.distance_to(Vector3.new(0, 0, 5))
    assert_equal [1.0, 2.0, 3.0], v.to_a
  end

  def test_bang_methods_change_the_vector_and_hand_it_back
    v = Vector3.new(1, 2, 3)
    assert v.add!(Vector3.new(1, 1, 1)).equal?(v)
    assert_equal Vector3.new(2, 3, 4), v
    v.sub!(Vector3.one).scale!(2)
    assert_equal Vector3.new(2, 4, 6), v
    assert_equal Vector3.new(7, 8, 9), v.set(7, 8, 9)
  end

  def test_normalize_in_place_leaves_a_zero_vector_zero
    assert_in_delta 1.0, Vector3.new(0, 0, 9).normalize!.length
    assert_equal Vector3.zero, Vector3.zero.normalize!
  end

  def test_a_copy_is_a_vector_of_its_own
    v = Vector3.new(1, 2, 3)
    copy = v.dup
    copy.add!(Vector3.one)
    assert_equal Vector3.new(1, 2, 3), v
    assert_equal Vector3.new(2, 3, 4), copy
  end

  def test_any_number_a_float_can_hold_makes_a_vector
    assert_equal [1.0, -2.5, 0.0], Vector3.new(1, -2.5, 0).to_a
    assert_equal [-3.0, 0.25, 7.0], Vector3.zero.set(-3, 0.25, 7).to_a
  end

  def test_no_vector_is_made_or_set_from_a_number_a_float_cannot_hold
    [0.0 / 0.0, 1.0 / 0.0, -1.0 / 0.0, 1e300, -1e300].each do |bad|
      assert_raises(ArgumentError) { Vector3.new(bad, 0, 0) }
      assert_raises(ArgumentError) { Vector3.new(0, 0, bad) }
      assert_raises(ArgumentError) { Vector3.one.set(0, bad, 0) }
      assert_raises(ArgumentError) { Vector3.one.scale!(bad) }
    end
  end

  def test_a_refused_set_leaves_the_vector_as_it_was
    v = Vector3.new(1, 2, 3)
    assert_raises(ArgumentError) { v.set(4, 0.0 / 0.0, 6) }
    assert_equal Vector3.new(1, 2, 3), v
    assert_equal Vector3.new(4, 5, 6), v.set(4, 5, 6)
  end

  BIG = 3e38 # fits a float; twice it does not

  def test_ordinary_arithmetic_is_unchanged
    assert_equal Vector3.new(2, 0.5, -3), Vector3.new(4, 1, -6) / 2
    assert_equal Vector3.new(0, 0, 1), Vector3.new(1, 0, 0).cross(Vector3.new(0, 1, 0))
    assert_equal Vector3.new(1, 0, 0), Vector3.new(1e-5, 0, 0).normalized
  end

  def test_no_arithmetic_makes_a_vector_past_what_a_float_holds
    big = Vector3.new(BIG, 0, 0)
    assert_raises(ArgumentError) { big + big }
    assert_raises(ArgumentError) { big - -big }
    assert_raises(ArgumentError) { big * 2 }
    assert_raises(ArgumentError) { big / 0.5 }
    assert_raises(ArgumentError) { Vector3.new(0, BIG, 0).cross(Vector3.new(0, 0, BIG)) }
  end

  def test_a_vector_is_never_divided_by_zero
    [0, 0.0, -0.0].each do |zero|
      assert_raises(ArgumentError) { Vector3.one / zero }
    end
  end

  def test_a_refused_change_in_place_leaves_every_number_as_it_was
    v = Vector3.new(1, BIG, 2)
    assert_raises(ArgumentError) { v.add!(Vector3.new(5, BIG, 5)) }
    assert_raises(ArgumentError) { v.sub!(Vector3.new(5, -BIG, 5)) }
    assert_raises(ArgumentError) { v.scale!(2) }
    assert_equal [1.0, BIG, 2.0], v.to_a
    assert_equal Vector3.new(2, BIG, 3), v.add!(Vector3.new(1, 0, 1))
  end

  def test_normalizing_keeps_its_contract_at_both_ends
    assert_equal Vector3.zero, Vector3.new(1e-7, 0, 0).normalized # too short to have a direction
    assert_in_delta 1.0, Vector3.new(BIG, BIG, BIG).normalized.length
    assert_in_delta 1.0, Vector3.new(BIG, -BIG, BIG).normalize!.length
  end

  def test_numbers_keep_their_precision
    assert_equal 0.1, Vector3.new(0.1, 0, 0).x
    assert_equal 0.0, Vector3.new.z
  end

  def test_only_a_vector_is_added_and_only_a_number_scales
    assert_raises(TypeError) { Vector3.one + 1 }
    assert_raises(TypeError) { Vector3.one.add!([1, 2, 3]) }
    assert_raises(TypeError) { Vector3.one * Vector3.one }
    refute Vector3.one == [1, 1, 1]
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

  def test_equal_vectors_are_one_key_of_a_hash
    a = Vector3.new(1, 2, 3)
    b = Vector3.new(1.0, 2.0, 3.0)
    assert a.eql?(b)
    assert_equal a.hash, b.hash
    assert_equal :found, { a => :found }[b]
    assert_equal 1, [a, b].uniq.size
    refute a.eql?(Vector3.new(1, 2, 4))
    refute a.eql?([1.0, 2.0, 3.0])
  end

  def test_a_negative_zero_is_the_same_key_as_zero
    assert_equal Vector3.zero, -Vector3.zero
    assert_equal Vector3.zero.hash, (-Vector3.zero).hash
  end

  # As with an Array: a key changed in place is no longer found by the value it had; a dup as the
  # key keeps its value whatever happens to the vector it was made from.
  def test_a_key_changed_in_place_is_not_found_by_its_old_value
    position = Vector3.new(1, 2, 3)
    kept = { position => :here }
    copied = { position.dup => :here }
    position.add!(Vector3.up)
    assert_nil kept[Vector3.new(1, 2, 3)]
    assert_equal :here, copied[Vector3.new(1, 2, 3)]
  end
end
