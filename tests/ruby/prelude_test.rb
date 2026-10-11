class FakeBehaviour < Behaviour; end

class PreludeTest < RedGemTest::Case
  def test_engine_modules_exist
    classes = [GameObject, Transform, CharacterController, Rigidbody, Vehicle, Behaviour]
    classes.each { |klass| assert klass.is_a?(Class) }
    [Scene, Camera, Input, UI, Debug, Game, Physics, TestInput].each { |mod| assert mod.is_a?(Module) }
  end

  def test_time_is_exposed_on_the_time_class
    assert_in_delta 0.0, Time.delta
    assert_equal 0, Time.frame
  end

  def test_behaviour_defaults_are_no_ops
    behaviour = FakeBehaviour.new
    assert_nil behaviour.start
    assert_nil behaviour.update(0.1)
    assert_nil behaviour.on_trigger_enter(nil)
  end

  def test_scene_lookup_on_empty_scene
    assert_nil Scene.find("Nothing")
    assert_equal [], Scene.find_all(:coin)
  end

  def test_unknown_input_axis_raises
    error = assert_raises(ArgumentError) { Input.axis(:nope) }
    assert error.message.include?("nope")
  end

  def test_unknown_key_raises
    assert_raises(ArgumentError) { Input.down?(:not_a_key) }
  end

  def test_ui_rejects_unknown_colors
    assert_raises(ArgumentError) { UI.text("hi", x: 0, y: 0, color: :ultraviolet) }
  end

  def test_test_input_rejects_unknown_keys
    assert_raises(ArgumentError) { TestInput.set(:not_a_key, true) }
  end

  def test_version
    assert_equal "0.1.0", RedGem::VERSION
  end
end
