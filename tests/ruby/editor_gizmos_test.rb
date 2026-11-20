# What the editor is told of an object's NPC parts while the game runs (EditorGizmos), on stand-in
# objects of an empty scene.
class EditorGizmosTest < RedGemTest::Case
  # A stand-in for an object of the scene.
  class GizmoObject
    attr_reader :behaviours

    def initialize = @behaviours = []
    def alive? = true
    def props = {}
    def get_component(klass) = @behaviours.find { |behaviour| behaviour.is_a?(klass) }
    def __add_behaviour(behaviour) = @behaviours << behaviour
  end

  RANGE = 3.5
  SIGHT = 9.0
  FOV = 80.0
  EYES = 1.2
  HEARING = 4.0
  PLAN = "06:00 bake, 12:30 sell".freeze

  def setup
    Perception.__scene_reset
    Interaction.__scene_reset
    @object = GizmoObject.new
  end

  def test_an_object_without_parts_has_no_settings
    assert_equal({}, EditorGizmos.__settings(@object))
  end

  def test_an_interactable_gives_its_range_and_prompt
    with(Interactable.new, range: RANGE, prompt: "door.open")
    assert_equal({ range: RANGE, prompt: "door.open" }, EditorGizmos.__settings(@object))
  end

  def test_a_perception_gives_its_sight_and_hearing
    with(Perception.new, sight: SIGHT, fov: FOV, eyes: EYES, hearing: HEARING)
    expected = { sight: SIGHT, fov: FOV, eyes: EYES, hearing: HEARING }
    assert_equal expected, EditorGizmos.__settings(@object)
  end

  def test_a_schedule_gives_its_plan_as_clock_times
    with(Schedule.new, plan: PLAN)
    assert_equal({ plan: ["06:00 bake", "12:30 sell"] }, EditorGizmos.__settings(@object))
  end

  def test_a_nav_agent_gives_where_it_goes_only_once_sent
    agent = with(NavAgent.new)
    assert_equal({}, EditorGizmos.__settings(@object))
    goal = Vector3.new(4, 0, 2)
    agent.instance_variable_set(:@destination, goal)
    agent.instance_variable_set(:@waypoints, [goal])
    assert_equal({ destination: goal, waypoints: [goal] }, EditorGizmos.__settings(@object))
  end

  private

  def with(behaviour, **settings)
    behaviour.__configure(settings)
    behaviour.__attach(@object)
    behaviour
  end
end
