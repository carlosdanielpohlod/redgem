# What a HeadLook turns the head toward (the nearest hero in range and in front, or its target),
# and how Animator finds a bone. The engine's bone aim is tested in tests/unit/head_aim_test.cpp;
# here the objects are stand-ins and the animation writes down what it is asked.
class LookTransform
  attr_accessor :position, :forward

  def initialize(position, forward = Vector3.forward)
    @position = position
    @forward = forward
  end
end

# The animation of a stand-in: what it was asked to look at.
class LookRecorder
  attr_reader :looked_at, :looked_away

  def bone!(name) = name
  def look_at(point, bone:, max_angle:) = @looked_at = [point, bone, max_angle]
  def look_away(bone:) = @looked_away = bone
end

# What a scene object answers that a HeadLook asks.
class LookingObject
  attr_reader :transform, :animation, :name
  attr_accessor :children

  def initialize(name, position)
    @name = name
    @transform = LookTransform.new(position)
    @animation = LookRecorder.new
    @behaviours = []
    @children = []
  end

  def alive? = true
  def props = {}
  def get_component(klass) = @behaviours.find { |behaviour| behaviour.is_a?(klass) }
  def __add_behaviour(behaviour) = @behaviours << behaviour
end

class HeadLookTest < RedGemTest::Case
  FRAME = 1.0 / 60
  NEAR = 2.0      # meters in front of the NPC
  FAR = 10.0      # past the default range
  EYES = Vector3.up * HeadLook::AIM

  def setup
    Interaction.__scene_reset
    @npc = LookingObject.new("Baker", Vector3.zero)
    @look = with(@npc, HeadLook.new)
    @hero = LookingObject.new("Hero", Vector3.new(0, 0, -NEAR))
    with(@hero, Interactor.new)
  end

  def test_it_looks_at_the_eyes_of_a_hero_in_front_and_near
    @look.update(FRAME)
    point, bone, max_angle = @npc.animation.looked_at
    assert_vector @hero.transform.position + EYES, point
    assert_equal Animator::HEAD, bone
    assert_in_delta HeadLook::ANGLE, max_angle
  end

  def test_it_looks_away_from_a_hero_too_far_or_behind
    @hero.transform.position = Vector3.new(0, 0, -FAR)
    @look.update(FRAME)
    assert_equal Animator::HEAD, @npc.animation.looked_away
    @hero.transform.position = Vector3.new(0, 0, NEAR)
    assert_nil @look.nearest_hero, "behind it"
  end

  def test_a_target_comes_before_the_heroes
    @look.target = Vector3.new(1, 2, 3)
    @look.update(FRAME)
    assert_vector Vector3.new(1, 2, 3), @npc.animation.looked_at.first
  end

  def test_nothing_turns_while_paused
    @look.update(0.0)
    assert_nil @npc.animation.looked_at
  end

  def test_a_bone_is_found_by_its_name_or_after_a_rigs_prefix
    assert Animator.bone_named?("Head", "Head")
    assert Animator.bone_named?("mixamorig:Head", "Head")
    refute Animator.bone_named?("HeadTop_End", "Head")
  end

  def test_an_animator_finds_a_bone_deep_under_its_character
    head = LookingObject.new("mixamorig:Head", Vector3.zero)
    neck = LookingObject.new("Neck", Vector3.zero)
    neck.children = [head]
    @npc.children = [neck]
    assert_equal head, Animator.new(@npc).bone("Head")
    assert_nil Animator.new(@npc).bone("Tail")
  end

  private

  def with(object, behaviour, **settings)
    behaviour.__configure(settings)
    behaviour.__attach(object)
    behaviour.start
    behaviour
  end
end
