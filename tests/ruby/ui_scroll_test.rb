# A scroll list: what it draws, and how keys, a gamepad's buttons, the wheel and a focus scroll it,
# with the keys and buttons pressed and the wheel turned said by the test. The pointer is at the
# screen's top-left corner, over the list.
class << UI
  # What the test presses this frame: [keys, gamepad buttons] pairs, as UI's constants name them.
  attr_accessor :pressed_now

  def pressed_any?(controls) = (pressed_now || []).include?(controls)
end

class << Input
  # How far the test turns the wheel this frame.
  attr_accessor :wheel_now

  def mouse_wheel = wheel_now || 0.0
end

class UiScrollTest < RedGemTest::Case
  ROW = 25
  ROWS = 10
  PAGE = 4                       # the rows that show at once
  LIST = { x: 0, y: 0, width: 200, height: PAGE * ROW, row_height: ROW }.freeze
  ON_THE_RIGHT = LIST.merge(x: 300).freeze # the same list, away from the pointer
  MOST = ROWS * ROW - PAGE * ROW # as far as it scrolls
  WHEEL_DOWN = -1.0

  def setup
    UI.pressed_now = []
    Input.wheel_now = nil
    @list = UI::ScrollList.new
  end

  def pressing(*controls)
    UI.pressed_now = controls
    yield
  ensure
    UI.pressed_now = []
  end

  # Draws the list (of ROWS rows unless said); returns each row drawn: [index, its box].
  def drawn(count = ROWS, **options)
    rows = []
    @list.draw(count, **LIST, **options) { |index, box| rows << [index, box] }
    rows
  end

  def test_it_draws_only_the_rows_that_show_where_they_are
    rows = drawn
    assert_equal [0, 1, 2, 3], rows.map(&:first)
    tops = rows.map { |_index, box| box[:y] }
    assert_equal [0, ROW, 2 * ROW, 3 * ROW], tops
    assert @list.scrolls?
  end

  def test_a_rows_box_leaves_its_gap_empty
    box = drawn(gap: 5).first.last
    assert_equal({ x: 0, y: 0, width: 200, height: ROW - 5 }, box)
  end

  def test_without_a_focus_up_and_down_scroll_a_row
    rows = pressing(UI::DOWN) { drawn }
    assert_equal ROW, @list.offset
    assert_equal [1, 0], [rows.first.first, rows.first.last[:y]]
    pressing(UI::UP) { drawn }
    assert_equal 0, @list.offset
  end

  def test_page_keys_and_a_gamepads_bumpers_go_a_page_and_it_stays_within_its_rows
    pressing(UI::UP) { drawn }
    assert_equal 0, @list.offset
    pressing(UI::PAGE_DOWN) { drawn }
    assert_equal PAGE * ROW, @list.offset
    pressing(UI::PAGE_DOWN) { drawn }
    assert_equal MOST, @list.offset
    assert_equal ROWS - 1, drawn.last.first
  end

  def test_a_list_shorter_than_its_box_does_not_scroll
    pressing(UI::DOWN) { drawn(2) }
    assert_equal 0, @list.offset
    refute @list.scrolls?
  end

  def test_the_wheel_scrolls_it_while_the_pointer_is_over_it
    Input.wheel_now = WHEEL_DOWN
    drawn
    assert_equal ROW, @list.offset
    @list.draw(ROWS, **ON_THE_RIGHT) { nil }
    assert_equal ROW, @list.offset, "the pointer is not over a list on the right"
  end

  def test_it_scrolls_to_the_row_the_focus_moves_to
    focus = UI::Focus.new
    focus.move_to(6)
    rows = drawn(focus: focus)
    assert_equal 7 * ROW - PAGE * ROW, @list.offset, "the focused row's bottom at the list's"
    assert_equal 6, rows.last.first
  end

  def test_the_wheel_takes_it_away_from_the_focus_until_the_focus_moves
    focus = UI::Focus.new
    focus.move_to(6)
    drawn(focus: focus)
    Input.wheel_now = -WHEEL_DOWN
    drawn(focus: focus)
    Input.wheel_now = nil
    drawn(focus: focus)
    assert_equal 6 * ROW - PAGE * ROW, @list.offset, "the list does not go back to a focus that has not moved"
  end

  def test_with_a_focus_arrows_are_the_focus_and_page_keys_move_it
    focus = UI::Focus.new
    pressing(UI::DOWN) { drawn(focus: focus) }
    assert_equal 0, @list.offset
    pressing(UI::PAGE_DOWN) { drawn(focus: focus) }
    assert focus.at?(PAGE)
    assert_equal ROW, @list.offset
    3.times { pressing(UI::PAGE_DOWN) { drawn(focus: focus) } }
    assert focus.at?(ROWS - 1), "never past the last row"
  end

  def test_rows_after_other_widgets_in_the_focus_start_at_first
    focus = UI::Focus.new
    focus.move_to(3)
    drawn(focus: focus, first: 1)
    assert_equal 0, @list.offset, "row 2 shows"
    focus.move_to(0)
    pressing(UI::PAGE_DOWN) { drawn(focus: focus, first: 1) }
    assert focus.at?(0), "the focus is not on a row: a page scrolls the list instead"
    assert_equal PAGE * ROW, @list.offset
  end

  def test_a_list_that_does_not_hear_input_stays_where_it_is
    pressing(UI::DOWN) { drawn(input: false) }
    assert_equal 0, @list.offset
  end

  def test_reset_goes_back_to_the_first_row
    pressing(UI::PAGE_DOWN) { drawn }
    @list.reset
    assert_equal 0, @list.offset
  end

  def test_rows_must_have_a_height
    assert_raises(ArgumentError) { @list.draw(ROWS, **LIST, row_height: 0) { nil } }
  end
end
