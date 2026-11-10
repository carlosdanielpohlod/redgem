# Word wrap, anchors and clipping, on a screen the test says and with letters as wide as a text is
# taken to be without a font to measure it. What UI draws is kept here instead of being drawn.
class << UI
  attr_accessor :drawn

  def width = UiLayoutTest::SCREEN_WIDTH
  def height = UiLayoutTest::SCREEN_HEIGHT
  def __text(text, x, y, *) = drawn << [:text, text, x, y]
  def __rect(x, y, width, height, *) = drawn << [:rect, x, y, width, height]
  def __clip(*box) = drawn << [:clip, *box]
  def __unclip = drawn << [:unclip]
end

class UiLayoutTest < RedGemTest::Case
  SCREEN_WIDTH = 800
  SCREEN_HEIGHT = 600
  SIZE = 20
  LETTER = 11            # how wide a letter (a byte of it) is taken to be at SIZE
  LINE = 25              # SIZE * UI::LINE_SPACING
  WIDE = 400             # room for every text of these tests on one line
  PHRASE = "one two three".freeze
  TWO_WORDS = 7 * LETTER # "one two"

  def setup
    UI.drawn = []
    UI.font = nil
  end

  def test_a_text_wraps_at_spaces_to_lines_no_wider_than_its_width
    assert_equal ["one two", "three"], UI.wrap(PHRASE, width: TWO_WORDS, size: SIZE)
    assert_equal [PHRASE], UI.wrap(PHRASE, width: PHRASE.size * LETTER, size: SIZE)
  end

  def test_runs_of_spaces_go_and_the_texts_own_line_breaks_stay
    assert_equal ["a b", "", "c"], UI.wrap("a   b\n\nc", width: WIDE, size: SIZE)
  end

  def test_a_word_longer_than_a_line_is_cut_between_letters
    assert_equal %w[go abc def gh], UI.wrap("go abcdefgh", width: 3 * LETTER, size: SIZE)
    assert_equal %w[a b], UI.wrap("ab", width: 1, size: SIZE), "a line always takes a letter"
  end

  def test_a_word_is_cut_between_whole_letters_however_many_bytes_each_takes
    lines = UI.wrap("çççç", width: 4 * LETTER, size: SIZE)  # two letters of two bytes each
    assert_equal %w[çç çç], lines
    assert_equal %w[a ç €], UI.letters_of("aç€")
  end

  def test_the_wrapping_of_a_text_is_kept_between_frames
    first = UI.wrap(PHRASE, width: TWO_WORDS, size: SIZE)
    assert first.equal?(UI.wrap(PHRASE, width: TWO_WORDS, size: SIZE))
    refute first.equal?(UI.wrap(PHRASE, width: TWO_WORDS + 1, size: SIZE)), "another width wraps again"
  end

  def test_a_texts_height_is_from_its_first_lines_top_to_its_last_lines_bottom
    assert_equal LINE + SIZE, UI.text_height(PHRASE, width: TWO_WORDS, size: SIZE)
    assert_equal SIZE, UI.text_height(PHRASE, size: SIZE)
    assert_equal 2 * LINE + SIZE, UI.text_height("a\nb\nc", size: SIZE)
    assert_equal 0, UI.text_height("", size: SIZE)
    assert_equal LINE, UI.line_height(SIZE)
  end

  def test_a_wrapped_text_is_drawn_a_line_at_a_time
    UI.text(PHRASE, x: 10, y: 20, width: TWO_WORDS, size: SIZE)
    assert_equal [[:text, "one two", 10, 20], [:text, "three", 10, 20 + LINE]], UI.drawn
  end

  def test_align_lays_each_line_out_around_x
    UI.text(PHRASE, x: WIDE, y: 0, width: TWO_WORDS, size: SIZE, align: :center)
    left = WIDE - TWO_WORDS / 2
    assert_equal [[:text, "one two", left, 0], [:text, "three", left + LETTER, LINE]], UI.drawn
    UI.drawn = []
    UI.text("abc", x: 100, y: 0, size: SIZE, align: :right)
    assert_equal [[:text, "abc", 100 - 3 * LETTER, 0]], UI.drawn
    assert_raises(ArgumentError) { UI.text("abc", x: 0, y: 0, align: :middle) }
  end

  def test_an_anchor_puts_x_and_y_from_its_point_of_the_screen
    assert_equal [SCREEN_WIDTH - 110, SCREEN_HEIGHT - 70],
                 UI.anchored(:bottom_right, x: -10, y: -20, width: 100, height: 50)
    assert_equal [(SCREEN_WIDTH - 100) / 2, (SCREEN_HEIGHT - 50) / 2],
                 UI.anchored(:center, x: 0, y: 0, width: 100, height: 50)
    assert_equal [SCREEN_WIDTH / 2 + 5, 6], UI.anchored(:top, x: 5, y: 6)
    assert_equal [5, 6], UI.anchored(nil, x: 5, y: 6, width: 100)
    assert_raises(ArgumentError) { UI.anchored(:middle, x: 0, y: 0) }
  end

  def test_a_rectangle_and_a_button_sit_at_their_anchor_by_its_corner
    UI.rect(anchor: :bottom_left, x: 10, y: -10, width: 200, height: 40)
    assert_equal [:rect, 10, SCREEN_HEIGHT - 50, 200, 40], UI.drawn.first
    UI.drawn = []
    UI.button("Map", anchor: :right, x: 0, y: 0, width: 200, height: 40)
    assert_includes_rect [SCREEN_WIDTH - 200, (SCREEN_HEIGHT - 40) / 2, 200, 40]
  end

  def test_an_anchored_text_sits_at_it_by_its_corner
    UI.text("abc", anchor: :top_right, x: -20, y: 20, size: SIZE)
    assert_equal [[:text, "abc", SCREEN_WIDTH - 3 * LETTER - 20, 20]], UI.drawn
    UI.drawn = []
    UI.text(PHRASE, anchor: :bottom, x: 0, y: 0, width: TWO_WORDS, size: SIZE)
    left = (SCREEN_WIDTH - TWO_WORDS) / 2
    top = SCREEN_HEIGHT - LINE - SIZE
    assert_equal [[:text, "one two", left, top], [:text, "three", left, top + LINE]], UI.drawn
  end

  def test_a_clip_cuts_what_its_block_draws_and_ends_after_it
    UI.clip(x: 10, y: 20, width: 100, height: 50) { UI.rect(x: 0, y: 0, width: 5, height: 5) }
    assert_equal [[:clip, 10, 20, 100, 50], [:rect, 0, 0, 5, 5], [:unclip]], UI.drawn
  end

  def test_a_clip_inside_another_is_where_both_are_and_the_outer_one_comes_back
    UI.clip(x: 0, y: 0, width: 100, height: 100) { UI.clip(x: 50, y: 80, width: 100, height: 100) { nil } }
    assert_equal [[:clip, 0, 0, 100, 100], [:clip, 50, 80, 50, 20], [:clip, 0, 0, 100, 100], [:unclip]], UI.drawn
  end

  def test_a_clip_ends_even_when_its_block_fails
    assert_raises(RuntimeError) { UI.clip(x: 0, y: 0, width: 10, height: 10) { raise "broken" } }
    assert_equal [:unclip], UI.drawn.last
    assert UI.clips.empty?
  end

  def test_the_pointer_outside_a_clip_is_over_nothing_drawn_in_it
    box = { x: 0, y: 0, width: 20, height: 20 } # the pointer is at the screen's corner
    assert UI.mouse_over?(**box)
    UI.clip(x: 10, y: 10, width: 50, height: 50) { refute UI.mouse_over?(**box) }
    UI.clip(x: 0, y: 0, width: 50, height: 50) { assert UI.mouse_over?(**box) }
  end

  def test_boxes_that_do_not_overlap_overlap_nowhere
    overlap = UI.overlap({ x: 0, y: 0, width: 10, height: 10 }, { x: 20, y: 5, width: 5, height: 5 })
    assert_equal 0, overlap[:width]
    assert_equal 5, overlap[:height]
  end

  private

  def assert_includes_rect(rect)
    assert UI.drawn.include?([:rect, *rect]), "no rectangle at #{rect.inspect} in #{UI.drawn.inspect}"
  end
end
