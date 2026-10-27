class AnimationGraphTest < RedGemTest::Case
  def graph
    result = Animation::Graph.new(initial: :move, parameters: { speed: 0.0, grounded: true })
    result.blend_1d(:move, parameter: :speed, clips: [[0.0, :Idle], [4.0, :Run]])
    result.clip(:air, :Jump, loop: false)
    result.transition(:move, :air, parameter: :grounded, equals: false, fade: 0.2)
    result
  end

  def test_graph_keeps_explicit_defaults_and_state_descriptions
    initial, parameters, states, transitions = graph.__definition
    assert_equal :move, initial
    assert parameters.include?([:speed, 0.0])
    assert parameters.include?([:grounded, true])
    assert_equal [:move, :speed, [[0.0, :Idle], [4.0, :Run]], true, 1.0], states[0]
    assert_equal [:move, :air, :grounded, :equal, false, 0.2], transitions[0]
  end

  def test_transition_needs_exactly_one_comparison
    assert_raises(ArgumentError) { graph.transition(:move, :air, parameter: :grounded) }
    assert_raises(ArgumentError) { graph.transition(:move, :air, parameter: :speed, above: 1.0, below: 2.0) }
  end

  def test_network_accepts_evaluated_graph_tracks_and_rejects_invalid_weights
    clip = ["Run", 0.25, 1.0, true, true]
    controller = [1.0, 0.0, true, [], [[clip, 1.0]]]
    assert Network::Said.animation?([0.0, 0.0, nil, nil, controller])
    controller[4][0][1] = 0.5
    assert !Network::Said.animation?([0.0, 0.0, nil, nil, controller])
  end

  def test_network_refuses_negative_times_and_oversized_graph_sides
    clip = ["Run", -0.25, 1.0, true, true]
    assert !Network::Said.clip?(clip)
    clip[1] = 0.25
    assert !Network::Said.graph?([1.0, 0.0, true, [], Array.new(5) { [clip, 0.2] }])
  end

  def test_network_accepts_four_grid_corners_but_rejects_five_even_with_normalized_weights
    clip = ["Run", 0.25, 1.0, true, true]
    side = Array.new(4) { [clip, 0.25] }
    assert Network::Said.graph?([1.0, 0.0, true, [], side])
    side << [clip, 0.0]
    assert !Network::Said.graph?([1.0, 0.0, true, [], side])
  end

  def test_two_dimensional_grid_keeps_its_parameter_pair_and_coordinates
    grid = Animation::Graph.new(initial: :move, parameters: { x: 0.0, y: 0.0 })
    points = [[[0, 0], :A], [[1, 0], :B], [[0, 1], :C], [[1, 1], :D]]
    grid.blend_2d(:move, parameters: %i[x y], clips: points)
    assert_equal [:move, %i[x y], points, true, 1.0], grid.__definition[2][0]
  end
end
