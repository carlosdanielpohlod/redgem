module Animation
  # A reusable description. Each Animator installs its own clocks and parameter values.
  class Graph
    UNSET = Object.new

    def initialize(initial:, parameters: {})
      @initial = initial
      @parameters = parameters
      @states = []
      @transitions = []
    end

    def clip(name, clip, loop: true, speed: Animation::NORMAL_SPEED)
      @states << [name, nil, [[0.0, clip]], loop, speed]
      self
    end

    def blend_1d(name, parameter:, clips:, loop: true, speed: Animation::NORMAL_SPEED)
      @states << [name, parameter, clips, loop, speed]
      self
    end

    def transition(from, to, parameter:, equals: UNSET, above: UNSET, below: UNSET, fade: Animator::NO_FADE)
      comparison, value = condition(equals, above, below)
      @transitions << [from, to, parameter, comparison, value, fade]
      self
    end

    def __definition = [@initial, @parameters.to_a, @states, @transitions]

    private

    def condition(equals, above, below)
      conditions = [[:equal, equals], [:above, above], [:below, below]].reject { |_key, value| value.equal?(UNSET) }
      raise ArgumentError, "a transition needs exactly one comparison" unless conditions.size == 1

      conditions.first
    end
  end
end

class Animator
  def use(graph)
    raise ArgumentError, "expected an Animation::Graph" unless graph.is_a?(Animation::Graph)

    @game_object.__animation_graph_use(graph.__definition)
  end

  def set(parameter, value) = @game_object.__animation_graph_set(parameter, value)
  def state = @game_object.__animation_graph_state
end
