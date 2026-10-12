class SceneTest < RedGemTest::Case
  def test_find_bang_raises_when_the_object_is_missing
    error = assert_raises(ArgumentError) { Scene.find!("Nothing") }
    assert_equal "the scene has no object named \"Nothing\"", error.message
  end

  def test_find_bang_names_who_asked
    error = assert_raises(ArgumentError) { Scene.find!("Nothing", wanted_by: SceneTest) }
    assert_equal "SceneTest: the scene has no object named \"Nothing\"", error.message
  end

  # The unit tests run in an empty scene, so the engine's own lookup stands aside for one name.
  def with_an_object_named(name, object)
    $stand_in = [name, object]
    class << Scene
      alias_method :find_in_the_scene, :find
      def find(name) = name == $stand_in.first ? $stand_in.last : find_in_the_scene(name)
    end
    yield
  ensure
    restore_find
  end

  def restore_find
    class << Scene
      alias_method :find, :find_in_the_scene
    end
  end

  def test_find_bang_returns_the_object
    object = Object.new
    with_an_object_named("Hero", object) do
      assert object.equal?(Scene.find!("Hero"))
      assert object.equal?(Behaviour.new.find!("Hero"))
      assert_raises(ArgumentError) { Scene.find!("Nothing") }
    end
    assert_nil Scene.find("Hero")
  end

  def test_behaviours_and_scene_scripts_say_who_they_are
    error = assert_raises(ArgumentError) { Behaviour.new.find!("Nothing") }
    assert error.message.start_with?("Behaviour: ")
    error = assert_raises(ArgumentError) { SceneScript.new.find!("Nothing") }
    assert_equal "SceneScript: the scene has no object named \"Nothing\"", error.message
  end
end
