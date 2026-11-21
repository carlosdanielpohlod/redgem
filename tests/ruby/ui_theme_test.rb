# The interface's theme (UI::Theme): its defaults, how layers are read and merged, what is wrong with
# a key or a value, and the files a menu's settings name. (How the menus use it: game_menu_theme_test.rb.)
class UiThemeTest < RedGemTest::Case
  def setup
    UI.theme = nil
  end

  def test_the_defaults_are_the_engines_own_look
    theme = UI::Theme.default
    assert_equal [280, 58, 16, 10], theme[:button].values_at(:width, :height, :gap, :radius)
    assert_equal %i[dark ruby gold], [theme[:colors, :button], theme[:colors, :hover], theme[:colors, :focus_ring]]
    assert_equal [:black, 170], [theme[:colors, :shade], theme[:colors, :shade_alpha]]
    assert_equal [:center, 0.46, :scene], [theme[:layout, :align], theme[:layout, :top], theme[:background, :kind]]
  end

  def test_a_layer_changes_only_what_it_says
    theme = UI::Theme.default.merged(UI::Theme.read!(colors: { button: :blue }, button: { radius: 0 }))
    assert_equal %i[blue ruby], [theme[:colors, :button], theme[:colors, :hover]]
    assert_equal [0, 280], [theme[:button, :radius], theme[:button, :width]]
    assert_equal :dark, UI::Theme.default[:colors, :button], "the defaults stay as they are"
  end

  def test_layers_over_layers_merge_section_by_section_and_replace_lists
    base = UI::Theme.read!(layout: { margins: 10, align: :left })
    over = UI::Theme.read!(layout: { margins: [1, 2, 3, 4] })
    merged = UI::Theme.merge(base, over)
    assert_equal({ margins: [1, 2, 3, 4], align: :left }, merged[:layout])
  end

  def test_values_written_as_game_json_has_them_are_read
    read = UI::Theme.read!("colors" => { "button" => "gold", "hover" => "#ff8800", "text" => [1, 2, 3] },
                           "layout" => { "align" => "right", "margins" => 24 },
                           "background" => { "camera_path" => { "points" => [[0, 1, 2]], "look_at" => [0, 0, 0] } })
    assert_equal [:gold, [255, 136, 0], [1, 2, 3]], read[:colors].values_at(:button, :hover, :text)
    assert_equal [:right, [24, 24, 24, 24]], read[:layout].values_at(:align, :margins)
    assert_vector Vector3.new(0, 1, 2), read[:background][:camera_path][:points].first
  end

  def test_nothing_is_a_value_only_where_a_setting_may_be_none
    assert_nil UI::Theme.read!(colors: { shade: nil })[:colors][:shade]
    error = assert_raises(ArgumentError) { UI::Theme.read!(colors: { button: nil }) }
    assert_includes_text error.message, "theme.colors.button must be a color"
  end

  def test_an_unknown_key_says_where_it_is_and_what_is_there
    error = assert_raises(ArgumentError) { UI.theme = { colors: { buton: :red } } }
    assert_includes_text error.message, "theme.colors.buton is not a theme setting"
    assert_includes_text error.message, "button"
  end

  def test_a_wrong_value_says_what_it_must_be
    _, problems = UI::Theme.read({ button: { radius: -1 }, layout: { align: :middle }, colors: { text: "nope" } })
    assert_equal 3, problems.size
    assert_includes_text problems[0], "button.radius must be a number, 0 or more, not -1"
    assert_includes_text problems[1], "layout.align must be one of left, center, right"
    assert_includes_text problems[2], "colors.text must be a color"
  end

  def test_no_margin_is_less_than_nothing
    _, problems = UI::Theme.read({ layout: { margins: [0, -10, 0, 0] } })
    assert_includes_text problems.first, "layout.margins must be a number, 0 or more, or [top, right, bottom, left]"
  end

  def test_a_section_must_be_a_table
    _, problems = UI::Theme.read({ colors: :red })
    assert_includes_text problems.first, "theme.colors must be a table of settings"
  end

  def test_a_color_name_it_does_not_know_is_refused
    assert UI::Theme::Kinds.bad?(UI::Theme::Kinds.read(:color, "not_a_color_anywhere"))
    assert UI::Theme::Kinds.bad?(UI::Theme::Kinds.read(:color, "#12345"))
    assert UI::Theme::Kinds.bad?(UI::Theme::Kinds.read(:color, [300, 0, 0]))
  end

  def test_ui_theme_changes_every_widget_and_nil_goes_back
    UI.theme = { colors: { button: :blue } }
    assert_equal :blue, UI.theme[:colors, :button]
    UI.theme = nil
    assert_equal :dark, UI.theme[:colors, :button]
  end

  def test_with_theme_draws_a_block_with_another_and_goes_back
    other = UI::Theme.default.merged(UI::Theme.read!(button: { padding: 3 }))
    inside = nil
    UI.with_theme(other) { inside = UI.padding }
    assert_equal [3, 14], [inside, UI.padding]
  end

  def test_the_menu_section_reads_a_theme_and_each_screens_own
    menu, problems = UI::Theme.read_menu("theme" => { "font" => "Display" },
                                         "screens" => { "title" => { "logo" => { "image" => "logo" } } })
    assert_equal [], problems
    assert_equal "Display", menu[:theme][:font]
    assert_equal "logo", menu[:screens]["title"][:logo][:image]
  end

  def test_the_menu_section_says_what_it_does_not_have
    _, problems = UI::Theme.read_menu("theme" => {}, "colours" => {}, "screens" => { "pause" => { "x" => 1 } })
    assert_includes_text problems[0], "colours is not a menu setting"
    assert_includes_text problems[1], "screens.pause.x is not a theme setting"
    _, wrong = UI::Theme.read_menu("screens" => [])
    assert_equal ["screens must be a table of settings (a Hash)"], wrong
  end

  def test_the_files_a_layer_names_are_listed_by_folder
    layer = UI::Theme.read!(font: %w[Body Emoji], heading_font: "Display", sounds: { pick: "click" },
                            button: { skin: "panel", icons: { play: "play_icon" } }, logo: { image: "logo" })
    files = UI::Theme.files_of(layer, UI::Theme::SCHEMA)
    expected = [%w[fonts Body], %w[fonts Emoji], %w[fonts Display], %w[sounds click], %w[images panel],
                %w[images play_icon], %w[images logo]]
    assert_equal expected.sort, files.sort
  end

  def test_a_game_without_a_menu_section_has_nothing_to_check
    assert_equal [[], []], UI::Theme.__check_settings
  end

  def test_a_buttons_state_says_what_shows_most
    assert_equal :normal, UI::ButtonState.new.name
    assert_equal :focused, UI::ButtonState.new(focused: true).name
    assert_equal :hover, UI::ButtonState.new(focused: true, hover: true).name
    assert_equal :pressed, UI::ButtonState.new(hover: true, pressed: true).name
    assert_equal :disabled, UI::ButtonState.new(hover: true, disabled: true).name
  end

  def test_a_disabled_button_is_grayed_and_a_pressed_one_takes_the_themes_pressed_color
    UI.theme = { colors: { pressed: :gold, disabled: :gray } }
    assert_equal UI::COLORS[:gold], UI.button_fill(UI::ButtonState.new(hover: true, pressed: true))
    assert_equal UI::COLORS[:gray], UI.button_fill(UI::ButtonState.new(disabled: true))
  end

  def test_a_nine_slice_keeps_its_corners_and_stretches_its_middle
    UI.theme = { button: { skin_edge: 10 } }
    columns = UI.slices(64, 100, 200)
    assert_equal [[0, 10, 100, 10], [10, 44, 110, 180], [54, 10, 290, 10]], columns
    assert_equal 4, UI.slices(8, 0, 40).first[1], "no corner larger than half the picture"
    refute UI.slice_shows?(UI.slices(20, 0, 40)[1]), "a picture of only corners has no middle"
    assert_equal [[0, 10, 0, 6], [10, 44, 6, 0], [54, 10, 6, 6]], UI.slices(64, 0, 12), "smaller, whole corners"
  end

  def test_shifted_moves_what_is_drawn_and_where_the_pointer_is
    x = UI.mouse_x
    UI.shifted(x: 30, alpha: 0.5) do
      assert_in_delta x - 30, UI.mouse_x
      assert_equal 85, UI.faded(170)
      assert_equal [40, 5, 10, 10], UI.shifted_box({ x: 10, y: 5, width: 10, height: 10 })
    end
    assert_equal UI::NO_SHIFT, UI.shift
  end

  def assert_includes_text(text, part)
    assert text.include?(part), "expected #{text.inspect} to include #{part.inspect}"
  end
end
