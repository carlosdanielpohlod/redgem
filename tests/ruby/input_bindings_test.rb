# Declaring actions and binding them anew from Ruby, as a settings screen would. (What the keys
# and gamepads then do, frame after frame, is tested with a game running:
# tests/integration/input_actions_test.cpp and tests/system/platformer/rebind_test.rb.)
class InputBindingsTest < RedGemTest::Case
  SECOND_PAD = 1

  def setup
    Input.reset_bindings
  end

  def test_the_built_in_names_are_there
    %i[jump fire run].each { |name| assert Input.actions.include?(name), name.to_s }
    assert_equal %i[horizontal vertical], Input.axes.sort
    assert_equal ["key:space", "pad:a"], Input.bindings(:jump)
  end

  def test_a_declared_action_lists_its_keys_then_its_buttons
    Input.define_action(:dash, keys: %i[e left_shift], buttons: [:b])
    assert_equal ["key:e", "key:left_shift", "pad:b"], Input.bindings(:dash)
    assert_equal ["pad:b"], Input.bindings(:dash, pad: SECOND_PAD)
  end

  def test_binding_by_key_by_button_or_by_text
    Input.bind(:jump, key: :k)
    Input.bind(:jump, button: :y)
    Input.bind(:jump, "key:j")
    assert_equal ["key:space", "key:k", "key:j", "pad:a", "pad:y"], Input.bindings(:jump)
    Input.reset_bindings(:jump)
    assert_equal ["key:space", "pad:a"], Input.bindings(:jump)
  end

  def test_an_axis_is_bound_side_by_side
    Input.unbind(:vertical, side: :positive)
    Input.bind(:vertical, key: :i, side: :positive)
    assert_equal ["key:i"], Input.bindings(:vertical)[:positive]
    assert_equal ["key:s", "key:down"], Input.bindings(:vertical)[:negative]
    assert_raises(ArgumentError) { Input.bind(:vertical, key: :i) }
  end

  def test_each_player_binds_their_own
    Input.bind(:run, key: :right_shift, pad: SECOND_PAD)
    assert_equal ["key:right_shift", "pad:b"], Input.bindings(:run, pad: SECOND_PAD)
    assert_equal ["key:left_shift", "pad:b"], Input.bindings(:run)
  end

  def test_unknown_names_and_bindings_are_refused
    assert_raises(ArgumentError) { Input.bindings(:fly) }
    assert_raises(ArgumentError) { Input.bind(:jump, key: :hyper) }
    assert_raises(ArgumentError) { Input.bind(:jump, "space") }
    assert_raises(ArgumentError) { Input.reset_bindings(:fly) }
    assert_raises(ArgumentError) { Input.bindings(:jump, pad: 9) }
  end

  def test_a_capture_waits_until_called_off
    Input.capture_next { |_binding, _pad| nil }
    assert Input.capturing?
    Input.cancel_capture
    refute Input.capturing?
  end
end
