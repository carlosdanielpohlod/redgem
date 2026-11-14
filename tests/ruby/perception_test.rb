# What a Perception sees (range, cone, line of sight) and hears (radius, loudness), and what its
# object's behaviours hear of it. The tests run in an empty scene: the objects are stand-ins, and
# a ray hits what the test puts in its way.
module Physics
  class << self
    attr_accessor :in_the_way

    def raycast(*_ray, **_filters) = in_the_way
  end
end

# Where a stand-in is and where it faces.
class SensedTransform
  attr_accessor :position, :forward

  def initialize(position, forward)
    @position = position
    @forward = forward
  end
end

# What a scene object answers that perception asks.
class SensedObject
  attr_reader :name, :transform, :children, :heard
  attr_accessor :parent, :alive, :behaviours

  def initialize(name, position, forward = Vector3.forward)
    @name = name
    @transform = SensedTransform.new(position, forward)
    @children = []
    @behaviours = []
    @heard = []
    @alive = true
  end

  def alive? = @alive
  def props = {}
  def get_component(klass) = @behaviours.find { |behaviour| behaviour.is_a?(klass) }
  def __add_behaviour(behaviour) = @behaviours << behaviour

  def __notify(callback, argument)
    @heard << [callback, argument]
    @behaviours.each { |behaviour| behaviour.__send__(callback, argument) }
  end
end

class PerceptionTest < RedGemTest::Case
  FRAME = 1.0 / 60
  AHEAD = 5.0             # meters in front of the guard
  BEYOND = 20.0           # past the default sight
  WIDE = 60.0             # degrees off the guard's forward: outside a 90-degree cone
  NARROW_FOV = 90.0
  LOUD = 2.0
  NEAR_NOISE = 8.0        # meters: inside the default hearing
  FAR_NOISE = 15.0        # past it, for a noise of loudness 1

  def setup
    Perception.__scene_reset
    Interaction.__scene_reset
    Physics.in_the_way = nil
    @guard = SensedObject.new("Guard", Vector3.zero)
    @perception = with(@guard, Perception.new, fov: NARROW_FOV)
    @hero = SensedObject.new("Hero", Vector3.new(0, 0, -AHEAD))
    with(@hero, Interactor.new)
  end

  def test_it_sees_a_hero_in_front_and_in_range
    assert @perception.sees?(@hero)
    @perception.update(FRAME)
    assert_equal [@hero], @perception.in_sight
    assert_equal [[:on_see, @hero]], @guard.heard
  end

  def test_it_does_not_see_past_its_range
    @hero.transform.position = Vector3.new(0, 0, -BEYOND)
    refute @perception.sees?(@hero)
  end

  def test_it_does_not_see_outside_its_cone
    @hero.transform.position = position_off_forward(WIDE)
    refute @perception.sees?(@hero)
    @hero.transform.position = Vector3.new(0, 0, AHEAD)
    refute @perception.sees?(@hero), "behind it"
  end

  def test_a_wall_hides_the_hero
    Physics.in_the_way = { object: SensedObject.new("Wall", Vector3.zero), distance: 1.0 }
    refute @perception.sees?(@hero)
  end

  def test_what_the_hero_carries_does_not_hide_it
    collider = SensedObject.new("Collider", @hero.transform.position)
    collider.parent = @hero
    Physics.in_the_way = { object: collider, distance: AHEAD - 1.0 }
    assert @perception.sees?(@hero)
  end

  def test_it_hears_when_a_hero_goes_out_of_sight_and_remembers_where
    @perception.update(FRAME)
    seen_at = @hero.transform.position
    @hero.transform.position = Vector3.new(0, 0, -BEYOND)
    @perception.update(FRAME)
    assert_equal [:on_lose_sight, @hero], @guard.heard.last
    refute @perception.sees_any?
    assert_vector seen_at, @perception.last_seen(@hero)
  end

  def test_it_notices_nothing_while_paused
    @perception.update(0.0)
    assert @guard.heard.empty?
  end

  def test_a_watch_tag_replaces_the_heroes
    tagged = with(SensedObject.new("Tagged", Vector3.zero), Perception.new, watch: "Nobody")
    assert tagged.watched.empty?, "the scene has nothing tagged Nobody"
  end

  def test_a_noise_is_heard_within_the_hearing_radius
    heard = Perception.noise(at: Vector3.new(NEAR_NOISE, 0, 0), source: @hero)
    assert_equal [@guard], heard
    callback, noise = @guard.heard.last
    assert_equal :on_hear, callback
    assert_in_delta NEAR_NOISE, noise.distance
    assert_equal @hero, noise.source
  end

  def test_loudness_carries_a_noise_further
    assert Perception.noise(at: Vector3.new(FAR_NOISE, 0, 0)).empty?
    assert_equal [@guard], Perception.noise(at: Vector3.new(FAR_NOISE, 0, 0), loudness: LOUD)
  end

  def test_a_listener_does_not_hear_its_own_noise
    assert Perception.noise(at: Vector3.zero, source: @guard).empty?
  end

  def test_a_brain_on_the_object_hears_of_what_it_perceives
    brain = with(@guard, AlertBrain.new)
    @perception.update(FRAME)
    assert_equal :alert, brain.state
    Perception.noise(at: Vector3.new(1, 0, 0))
    assert_equal :listening, brain.state
  end

  def test_wrong_settings_raise_as_it_starts
    assert_raises(ArgumentError) { with(SensedObject.new("Bad", Vector3.zero), Perception.new, fov: 400) }
    assert_raises(ArgumentError) { with(SensedObject.new("Bad", Vector3.zero), Perception.new, sight: -1) }
    assert_raises(ArgumentError) { Perception.noise(at: Vector3.zero, loudness: "loud") }
  end

  private

  # A behaviour put on a stand-in with its settings, started.
  def with(object, behaviour, **settings)
    behaviour.__configure(settings)
    behaviour.__attach(object)
    behaviour.start
    behaviour
  end

  def position_off_forward(degrees)
    radians = degrees / Perception::DEGREES_PER_RADIAN
    Vector3.new(Math.sin(radians), 0, -Math.cos(radians)) * AHEAD + Vector3.up * (Perception::EYES - Perception::AIM)
  end
end

# Turns to what its Perception tells it.
class AlertBrain < Brain
  state :idle
  state :alert
  state :listening
  transition from: :idle, to: :alert, on: :see
  transition from: :alert, to: :listening, on: :hear, if: ->(noise) { noise.loudness.positive? }
end
