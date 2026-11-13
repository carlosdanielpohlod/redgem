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

  def test_the_mouse_is_bound_by_its_buttons_and_its_wheel_after_the_keys
    Input.define_action(:shoot, :f, "pad:right_bumper", mouse: %i[left wheel_up])
    assert_equal ["key:f", "mouse:left", "mouse:wheel_up", "pad:right_bumper"], Input.bindings(:shoot)
    Input.bind(:jump, mouse: :side)
    Input.bind(:jump, "mouse:right")
    assert_equal ["key:space", "mouse:side", "mouse:right", "pad:a"], Input.bindings(:jump)
    Input.unbind(:jump, mouse: %i[side right])
    assert_equal ["key:space", "pad:a"], Input.bindings(:jump)
    assert_raises(ArgumentError) { Input.bind(:jump, mouse: :thumb) }
  end

  def test_the_full_keyboard_binds_by_name
    Input.define_action(:quick_save, :f5, :kp_enter, :page_up, :left_bracket, :right_alt, :caps_lock)
    expected = %w[key:f5 key:kp_enter key:page_up key:left_bracket key:right_alt key:caps_lock]
    assert_equal expected, Input.bindings(:quick_save)
  end

  def test_an_axis_is_bound_side_by_side
    Input.unbind(:vertical, side: :positive)
    Input.bind(:vertical, key: :i, side: :positive)
    assert_equal ["key:i"], Input.bindings(:vertical)[:positive]
    assert_equal ["key:s", "key:down"], Input.bindings(:vertical)[:negative]
    assert_raises(ArgumentError) { Input.bind(:vertical, key: :i) }
  end

  def test_an_axis_says_which_stick_moves_it_the_same_for_every_player
    assert_equal :left_x, Input.axis_stick(:horizontal)
    assert_equal :left_y, Input.axis_stick(:vertical)
    assert_nil Input.axis_stick(:jump)
    assert_raises(ArgumentError) { Input.axis_stick(:fly) }
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

  def test_gamepads_cannot_rumble_yet_and_rumble_says_so
    refute Input.rumble?
    refute Input.rumble(Input::FIRST_PAD, 0.5, 0.25)
    refute Input.rumble(nil, 1, 0)
  end

  def test_rumble_checks_its_player_its_strength_and_its_time
    assert_raises(ArgumentError) { Input.rumble(Input::PLAYERS, 0.5, 1) }
    assert_raises(ArgumentError) { Input.rumble(Input::FIRST_PAD, 1.5, 1) }
    assert_raises(ArgumentError) { Input.rumble(Input::FIRST_PAD, -0.1, 1) }
    assert_raises(ArgumentError) { Input.rumble(Input::FIRST_PAD, 0.5, -1) }
    assert_raises(ArgumentError) { Input.rumble(Input::FIRST_PAD, 0.5, nil) }
    assert_raises(ArgumentError) { Input.rumble(Input::FIRST_PAD, 0.5, Float::NAN) }
  end
end
