# How the menus take their theme (see game_menu_theme.rb): which layer wins, where the theme puts
# the buttons and the heading, and what a game's own drawing of the buttons is given. (How it looks:
# the pictures of tests/system_fixtures/menu_look.)
class GameMenuThemeTest < RedGemTest::Case
  def setup
    UI.theme = nil
    GameMenu.theme = nil
  end

  def menu
    menu = GameMenu.new
    menu.__configure({ title: "Coin Rush", start_open: false, journal: false })
    menu.start
    menu
  end

  def test_a_menu_with_no_theme_has_the_engines_look
    assert_equal UI::Theme.default.to_h, menu.theme_of(:title).to_h
  end

  def test_each_layer_is_over_the_last
    UI.theme = { colors: { button: :blue, hover: :green } }
    GameMenu.theme = { colors: { hover: :gold } }
    themed = menu
    themed.screens[:title].theme = { colors: { focused: :orange } }
    assert_equal %i[blue gold orange], themed.theme_of(:title)[:colors].values_at(:button, :hover, :focused)
    assert_equal :ruby, themed.theme_of(:pause)[:colors, :focused], "a screen's own is only its own"
  ensure
    GameMenu.theme = nil
  end

  def test_a_theme_changed_later_is_seen_at_once
    themed = menu
    assert_equal :dark, themed.theme_of(:pause)[:colors, :button]
    GameMenu.theme = { colors: { button: :red } }
    assert_equal :red, themed.theme_of(:pause)[:colors, :button]
  ensure
    GameMenu.theme = nil
  end

  def test_a_wrong_theme_is_refused_where_it_is_given
    error = assert_raises(ArgumentError) { GameMenu.theme = { layout: { align: :up } } }
    assert error.message.include?("theme.layout.align must be one of left, center, right")
    assert_raises(ArgumentError) { menu.screens[:title].theme = { logo: { imgae: "logo" } } }
  end

  def test_the_buttons_line_up_as_the_layout_aligns_them
    GameMenu.theme = { layout: { align: :left, margins: [0, 0, 0, 40] } }
    left = menu
    left.show(:pause)
    assert_equal left.scaled(40), left.button_box(0)[0]
    GameMenu.theme = { layout: { align: :right, margins: [0, 30, 0, 0] } }
    assert_equal UI.width - left.scaled(30), left.button_box(0).values_at(0, 2).sum
  ensure
    GameMenu.theme = nil
  end

  def test_the_buttons_size_and_gap_are_the_themes
    GameMenu.theme = { button: { width: 400, height: 40, gap: 4 } }
    sized = menu
    sized.show(:title)
    _, y, width, height = sized.button_box(0)
    assert_equal [sized.scaled(400), sized.scaled(40)], [width, height]
    assert_equal y + height + sized.scaled(4), sized.button_box(1)[1]
  ensure
    GameMenu.theme = nil
  end

  def test_with_a_max_width_the_buttons_grow_to_their_widest_label
    GameMenu.theme = { button: { width: 40, max_width: 600 } }
    grown = menu
    grown.show(:title)
    width = grown.button_box(0)[2]
    assert width > grown.scaled(40), "as wide as Settings needs"
    assert width <= grown.scaled(600)
  ensure
    GameMenu.theme = nil
  end

  def test_a_screens_own_top_wins_over_where_the_screen_puts_its_buttons
    opened = menu
    opened.screens[:settings].theme = { layout: { top: 0.5 } }
    opened.show(:settings)
    assert_equal (UI.height * 0.5).to_i, opened.button_box(0)[1]
  end

  def test_a_game_draws_the_buttons_itself_and_the_menu_still_hears_them
    drawn = []
    custom = menu
    custom.on_draw_button { |label, box, state| drawn << [label, box.keys.sort, state.id, state.focused?] }
    custom.show(:title)
    custom.update(0.0)
    assert_equal ["Play", %i[height width x y], :play, true], drawn.first
    assert_equal %w[Play Settings Quit], drawn.map(&:first)
  end

  def test_a_screens_drawing_wins_over_the_menus
    drawn = []
    custom = menu
    custom.on_draw_button { |label, _box, _state| drawn << [:menu, label] }
    custom.screens[:pause].on_draw_button { |label, _box, _state| drawn << [:pause, label] }
    custom.screens[:pause].on_draw_background { drawn << %i[pause background] }
    custom.show(:pause)
    custom.update(0.0)
    assert_equal [%i[pause background], true], [drawn.shift, drawn.all? { |by, _| by == :pause }]
  end

  def test_an_option_says_its_id_and_whether_it_can_be_picked
    screen = GameMenu::Screen.new
    open = true
    screen.add_option("Online", id: :online, enabled: -> { open }) { :joined }
    extra = screen.option_extras.first
    assert_equal [:online, false], [extra[:id], GameMenu::Screen.disabled?(extra)]
    open = false
    assert GameMenu::Screen.disabled?(extra)
    refute GameMenu::Screen.disabled?({})
  end

  def test_a_heading_at_the_top_sits_under_the_top_margin
    GameMenu.theme = { layout: { heading: :top, heading_top: 20, margins: [10, 0, 0, 0] } }
    placed = menu
    placed.show(:pause)
    assert_equal placed.scaled(10) + placed.scaled(20),
                 placed.__send__(:heading_y, placed.screens[:pause], :top, placed.theme_of(:pause))
  ensure
    GameMenu.theme = nil
  end

  def test_a_camera_path_goes_through_its_points_and_back
    points = [Vector3.new(0, 0, 0), Vector3.new(10, 0, 0)]
    flight = { points: points, look_at: nil, seconds: 4.0 }
    path = GameMenu::CameraPath.new(flight, nil, Camera.main&.transform || Transformless.new)
    assert_vector points[0], path.at(0.0)
    assert_vector Vector3.new(5, 0, 0), path.at(1.0)
    assert_vector points[1], path.at(2.0)
    assert_vector points[0], path.at(4.0)
  end

  # Where a camera would be, for a path made with no scene around it.
  class Transformless
    def position = Vector3.zero
    def rotation = Vector3.zero
  end
end
