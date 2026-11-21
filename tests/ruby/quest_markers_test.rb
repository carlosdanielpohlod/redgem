# Where the tracked quest's objectives are shown (quest_markers.rb, quest_markers_marks.rb), without
# a game: a pointer's place on the edge of the screen, and the marks worked out through a stand-in
# camera at the origin that looks down -z, on a screen WIDTH by HEIGHT. Also on a client, whose
# quests are the host's.
class QuestMarkersTest < RedGemTest::Case
  WIDTH = 800
  HEIGHT = 600
  MARGIN = 50
  FOCAL = 400.0           # units across the screen a meter at a meter's distance takes
  # Someone or something of the scene, as the markers see it.
  Thing = Struct.new(:transform)
  Spot = Struct.new(:position, :right, :forward)
  PLACES = { "Well" => Vector3.new(0, -1.5, -10), "Baker" => Vector3.new(0, -1.5, 10) }.freeze
  CAMERA = Thing.new(Spot.new(Vector3.zero, Vector3.right, Vector3.forward))
  NEAR_PLANE = 0.1        # meters: nearer, the stand-in camera shows nothing, as the engine's

  class << Camera
    def main = QuestMarkersTest::CAMERA

    # (Nil behind the camera, as the engine's.)
    def world_to_screen(point)
      depth = -point.z
      return nil if depth < QuestMarkersTest::NEAR_PLANE

      [QuestMarkersTest::WIDTH / 2 + point.x / depth * QuestMarkersTest::FOCAL,
       QuestMarkersTest::HEIGHT / 2 - point.y / depth * QuestMarkersTest::FOCAL]
    end
  end

  class << Scene
    def find(name)
      place = QuestMarkersTest::PLACES[name]
      place && QuestMarkersTest::Thing.new(QuestMarkersTest::Spot.new(place))
    end
  end

  class << UI
    def width = QuestMarkersTest::WIDTH
    def height = QuestMarkersTest::HEIGHT
  end

  class << Network
    attr_accessor :pretend_client

    def online? = pretend_client == true
    def host? = pretend_client != true
  end

  ERRAND = {
    "title" => "errand.title", "start" => "go",
    "stages" => {
      "go" => { "text" => "errand.go", "objectives" => [
        { "go_to" => "Well", "text" => "errand.well" },
        { "collect" => "water", "text" => "errand.water" },
        { "deliver" => "water", "to" => "Baker", "text" => "errand.deliver" }
      ], "next" => "done" },
      "done" => { "text" => "errand.done", "end" => true }
    }
  }.freeze
  HERO = Vector3.new(0, 0, 0)
  FROZEN = 0.0

  def setup
    Network.pretend_client = false
    [Quests, QuestMarkers].each(&:__scene_reset)
    QuestMarkers.enabled = true
    QuestMarkers.reset_style
    GameVariables.replace({})
    Quests.track(nil)
    Quests.instance_variable_get(:@definitions)["errand"] = Quests::Definition.new("errand", ERRAND)
  end

  def marks
    QuestMarkers.__update(HERO, FROZEN)
    QuestMarkers.marks
  end

  def kinds = marks.map { |mark| mark.objective.kind }

  def mark_of(target) = marks.find { |mark| [mark.objective.target, mark.objective.recipient].include?(target) }

  def test_a_pointer_sits_on_the_edge_on_the_way_to_its_place
    assert_equal [WIDTH - MARGIN, HEIGHT / 2], QuestMarkers.edge_point([1.0, 0.0], WIDTH, HEIGHT, MARGIN)
    assert_equal [WIDTH / 2, MARGIN], QuestMarkers.edge_point([0.0, -1.0], WIDTH, HEIGHT, MARGIN)
    diagonal = Math.sqrt(0.5)
    x, y = QuestMarkers.edge_point([-diagonal, diagonal], WIDTH, HEIGHT, MARGIN)
    assert_in_delta HEIGHT - MARGIN, y, 1e-3, "the screen is wider than tall: a diagonal meets the bottom"
    assert_in_delta WIDTH / 2 - (HEIGHT / 2 - MARGIN), x, 1e-3
  end

  def test_a_margin_larger_than_the_screen_puts_the_pointer_in_the_middle
    assert_equal [WIDTH / 2, HEIGHT / 2], QuestMarkers.edge_point([1.0, 0.0], WIDTH, HEIGHT, WIDTH)
  end

  def test_a_point_is_on_the_screen_inside_its_edges_and_not_behind
    assert QuestMarkers.on_screen?([0, HEIGHT], WIDTH, HEIGHT)
    refute QuestMarkers.on_screen?([WIDTH + 1, 0], WIDTH, HEIGHT)
    refute QuestMarkers.on_screen?(nil, WIDTH, HEIGHT)
  end

  def test_nothing_shows_before_a_quest_is_under_way
    assert_equal [], marks
  end

  def test_go_to_and_deliver_objectives_are_marked_and_collect_is_not
    Quests.start(:errand)
    assert_equal %i[go_to deliver], kinds
  end

  def test_a_place_in_front_is_a_marker_over_it
    Quests.start(:errand)
    well = mark_of("Well")
    refute well.pointer
    assert_equal [WIDTH / 2, HEIGHT / 2], [well.x, well.y], "a marker floats its height over the place"
    assert_in_delta PLACES["Well"].length, well.meters
  end

  def test_a_place_behind_the_camera_is_a_pointer_at_the_bottom_edge
    Quests.start(:errand)
    baker = mark_of("Baker")
    assert baker.pointer
    assert_equal QuestMarkers::DOWN, baker.direction
    assert_in_delta HEIGHT - QuestMarkers::STYLE[:margin], baker.y
  end

  def test_a_place_off_to_the_side_points_from_that_edge
    Quests.start(:errand)
    QuestMarkers.locate { |objective| Vector3.new(100, -1.5, -10) if objective.kind == :go_to }
    well = mark_of("Well")
    assert well.pointer
    assert_in_delta WIDTH - QuestMarkers::STYLE[:margin], well.x
    assert_in_delta HEIGHT / 2, well.y
  end

  def test_a_place_too_near_for_the_camera_to_show_is_marked_in_the_middle
    Quests.start(:errand)
    QuestMarkers.locate { |objective| Vector3.new(0, -1.5, -NEAR_PLANE / 2) if objective.kind == :go_to }
    well = mark_of("Well")
    refute well.pointer
    assert_equal [WIDTH / 2, HEIGHT / 2], [well.x, well.y]
  end

  def test_a_done_objective_is_no_longer_marked
    Quests.start(:errand)
    Quests.reached("Well")
    assert_equal %i[deliver], kinds
  end

  def test_the_game_can_turn_them_off
    Quests.start(:errand)
    QuestMarkers.enabled = false
    assert_equal [], marks
  end

  def test_the_game_can_restyle_them_and_draw_them_itself
    Quests.start(:errand)
    assert_equal 30, QuestMarkers.style(size: 30)[:size]
    assert_raises(ArgumentError) { QuestMarkers.style(colour: :red) }
    drawn = []
    QuestMarkers.draw { |mark| drawn << mark.objective.kind }
    QuestMarkers.__update(HERO, 1.0)
    assert_equal %i[go_to deliver], drawn
  end

  def test_on_a_client_they_follow_the_hosts_quests
    GameVariables.replace({ "quest.errand" => "go", "quest.errand#0" => 1 })   # as the host told them
    Network.pretend_client = true
    assert_equal %i[deliver], kinds
  ensure
    Network.pretend_client = false
  end
end
