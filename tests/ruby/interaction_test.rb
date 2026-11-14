# What an Interactor picks to use (range, angle, priority, sight, enabled), and who hears of a use.
# The tests run in an empty scene: the objects are stand-ins, and a ray hits what the test puts in
# its way.
module Physics
  class << self
    attr_accessor :in_the_way, :ignored

    def raycast(*_ray, ignore: nil, **_filters)
      @ignored = ignore
      in_the_way
    end
  end
end

# Where a stand-in is and where it faces.
class StandInTransform
  attr_accessor :position, :forward

  def initialize(position, forward)
    @position = position
    @forward = forward
  end
end

# What a scene object answers that interaction asks.
class StandInObject
  attr_reader :name, :transform, :behaviours, :children
  attr_accessor :parent, :alive

  def initialize(name, position, forward = Vector3.forward)
    @name = name
    @transform = StandInTransform.new(position, forward)
    @behaviours = []
    @children = []
    @alive = true
  end

  def alive? = @alive
  def props = {}
  def get_component(klass) = @behaviours.find { |behaviour| behaviour.is_a?(klass) }
  def __add_behaviour(behaviour) = @behaviours << behaviour
  def __notify(callback, actor) = @behaviours.each { |behaviour| behaviour.__send__(callback, actor) }

  def carry(child)
    child.parent = self
    @children << child
    child
  end
end

# Writes down what it hears.
class UseRecorder < Behaviour
  attr_reader :heard

  def initialize
    super
    @heard = []
  end

  def on_interact(actor) = @heard << [:interact, actor]
  def on_focus(actor) = @heard << [:focus, actor]
  def on_blur(actor) = @heard << [:blur, actor]
end

class InteractionTest < RedGemTest::Case
  NEAR = 1.0                # meters in front of the actor
  FAR = 5.0                 # past the default range
  NARROW = 60.0             # degrees
  HIGH = 5
  SIDEWAYS = 45.0           # degrees off the actor's forward
  BEHIND = Vector3.new(0, 0, 1)
  FRAME = 1.0 / 60

  def setup
    Interaction.__scene_reset
    Input.reset_bindings
    Physics.in_the_way = nil
    @actor = StandInObject.new("Hero", Vector3.zero)
  end

  def interactable(name, position, **settings)
    target = Interactable.new
    target.__configure(settings)
    target.__attach(StandInObject.new(name, position))
    target
  end

  def ahead(meters) = Vector3.forward * meters

  def recorder_on(target)
    recorder = UseRecorder.new
    recorder.__attach(target.game_object)
    recorder
  end

  def picks(expected, candidates, message = "picks #{expected&.name}")
    assert Interaction.best_for(@actor, candidates).equal?(expected), message
  end

  def test_the_nearest_in_range_is_picked
    near = interactable("Near", ahead(NEAR))
    further = interactable("Further", ahead(NEAR * 1.5))
    far = interactable("Far", ahead(FAR))
    picks near, [far, further, near]
    picks further, [far, further]
    picks nil, [far]
    far.range = FAR + 1
    picks far, [far], "a longer range reaches it"
  end

  def test_an_angle_keeps_to_what_is_in_front
    behind = interactable("Behind", BEHIND, angle: NARROW)
    picks nil, [behind]
    behind.angle = nil
    picks behind, [behind], "with no angle, any direction"
  end

  def test_an_angle_is_measured_from_where_the_actor_faces
    turned = Math.sin(SIDEWAYS * Math::PI / 180)
    aside = interactable("Aside", Vector3.new(turned, 0, -turned), angle: NARROW)
    assert_in_delta SIDEWAYS, Interaction.angle_to(@actor, aside)
    picks aside, [aside]
    aside.angle = SIDEWAYS / 2
    picks nil, [aside]
  end

  def test_the_highest_priority_wins_before_the_nearest
    near = interactable("Near", ahead(NEAR))
    important = interactable("Important", ahead(NEAR * 1.5), priority: HIGH)
    picks important, [near, important]
    important.priority = -HIGH
    picks near, [near, important]
  end

  def test_something_in_the_way_hides_a_target
    target = interactable("Door", ahead(NEAR))
    wall = StandInObject.new("Wall", ahead(NEAR / 2))
    Physics.in_the_way = { object: wall, distance: NEAR / 2 }
    picks nil, [target]
    Physics.in_the_way = { object: wall, distance: NEAR * 2 }
    picks target, [target], "something behind it"
  end

  def test_a_ray_that_reaches_the_target_sees_it
    target = interactable("Door", ahead(NEAR))
    Physics.in_the_way = { object: target.game_object, distance: NEAR * 0.9 }
    picks target, [target]
    assert_equal [@actor], Physics.ignored, "the actor does not hide what it looks at"
  end

  def test_what_is_under_the_target_is_the_target
    target = interactable("Guard", ahead(NEAR))
    body = target.game_object.carry(StandInObject.new("GuardBody", ahead(NEAR)))
    Physics.in_the_way = { object: body, distance: NEAR / 2 }
    picks target, [target], "a character's collider on a child"
  end

  def test_what_the_actor_carries_hides_nothing
    sword = @actor.carry(StandInObject.new("Sword", ahead(NEAR / 4)))
    target = interactable("Door", ahead(NEAR))
    Interaction.best_for(@actor, [target])
    assert_equal [@actor, sword], Physics.ignored
  end

  def test_a_disabled_target_is_never_picked
    off = interactable("Off", ahead(NEAR), enabled: false)
    on = interactable("On", ahead(NEAR * 1.5))
    picks on, [off, on]
    off.enabled = true
    picks off, [off, on]
    [on, off].each { |target| target.enabled = false }
    picks nil, [off, on]
  end

  def test_a_use_reaches_the_targets_behaviours_then_the_game
    target = interactable("Lever", ahead(NEAR))
    recorder = recorder_on(target)
    heard = []
    Interaction.on(:interact) { |actor, object| heard << [actor, object] }
    assert Interaction.interact(@actor, target.game_object)
    assert_equal [[:interact, @actor]], recorder.heard
    assert_equal [[@actor, target.game_object]], heard
  end

  def test_a_target_out_of_reach_is_not_used
    target = interactable("Lever", ahead(FAR))
    recorder = recorder_on(target)
    refute Interaction.interact(@actor, target)
    assert_equal [], recorder.heard
    refute Interaction.interact(@actor, StandInObject.new("Rock", ahead(NEAR))), "nothing to use on it"
  end

  def test_the_host_allows_a_client_a_little_more
    target = interactable("Lever", ahead(Interactable::RANGE + Interaction::NETWORK_SLACK / 2), angle: NARROW)
    picks nil, [target]
    assert Interaction.best_for(@actor, [target], lenient: true).equal?(target)
    @actor.transform.forward = Vector3.back
    assert_nil Interaction.best_for(@actor, [target], lenient: true), "turned away is too far still"
  end

  def test_the_host_uses_the_one_in_reach_of_those_with_the_name_asked
    far = interactable("Chest", ahead(FAR))
    near = interactable("Chest", ahead(NEAR))
    recorders = [far, near].map { |target| recorder_on(target) }
    assert Interaction.__asked(@actor, [far.game_object, near.game_object])
    assert_equal [[], [[:interact, @actor]]], recorders.map(&:heard)
    refute Interaction.__asked(@actor, [far.game_object]), "out of reach, even with the slack"
  end

  def test_settings_that_are_no_numbers_raise_as_they_are_set
    target = interactable("Sign", ahead(NEAR))
    assert_raises(ArgumentError) { target.range = -1 }
    assert_raises(ArgumentError) { target.angle = "wide" }
    assert_raises(ArgumentError) { target.priority = Float::NAN }
    assert_raises(ArgumentError) { interactable("Sign", ahead(NEAR), angle: "wide").start }
  end

  def test_the_events_are_named
    error = assert_raises(ArgumentError) { Interaction.on(:poke) { nil } }
    assert error.message.include?(":poke")
  end

  def new_interactor(**settings)
    interactor = Interactor.new
    interactor.__configure(settings)
    interactor.__attach(@actor)
    interactor
  end

  def test_the_focus_comes_and_goes
    target = interactable("Sign", ahead(NEAR)).tap(&:start)
    recorder = recorder_on(target)
    interactor = new_interactor(prompt: false)
    interactor.update(FRAME)
    assert target.equal?(interactor.target)
    target.enabled = false
    assert_nil interactor.tap { |looking| looking.update(FRAME) }.target
    assert_equal [[:focus, @actor], [:blur, @actor]], recorder.heard
  end

  def test_a_destroyed_target_hears_no_blur
    target = interactable("Coin", ahead(NEAR)).tap(&:start)
    recorder = recorder_on(target)
    interactor = new_interactor(prompt: false)
    interactor.update(FRAME)
    target.game_object.alive = false
    interactor.update(FRAME)
    assert_equal [[:focus, @actor]], recorder.heard
  end

  def test_the_prompt_names_the_key
    target = interactable("Door", ahead(NEAR), prompt: "door.open")
    interactor = new_interactor
    assert_equal "[E] door.open", interactor.prompt_label(target), "a key with no text shows as itself"
    assert_equal "[E] Interact", interactor.prompt_label(interactable("Thing", ahead(NEAR)))
    Input.unbind(Interaction::ACTION, key: :e)
    Input.bind(Interaction::ACTION, key: :f)
    assert_equal "[F] door.open", interactor.prompt_label(target), "as the player bound it"
  end
end
