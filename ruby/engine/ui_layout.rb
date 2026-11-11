# Where UI draws things on screens of any size, and how long texts are laid out: anchors, word
# wrap and clipping.
#
# An anchor is a point of the screen: a corner, the middle of an edge or the middle. Given one,
# x and y are offsets from it (right and down, as always), and what is drawn sits at it by its
# own matching corner, so a HUD stays in place at any resolution:
#
#   UI.text("Lives: #{@lives}", anchor: :top_right, x: -20, y: 20)      # its top-right corner
#   UI.button("Map", anchor: :bottom, x: 0, y: -24, width: 200, height: 56)
#
# A text given a width wraps at word boundaries (a word longer than a line is cut between
# letters); UI.text_height says how tall it is:
#
#   UI.rect(x: 40, y: 400, width: 560, height: UI.text_height(@line, width: 520, size: 24) + 40)
#   UI.text(@line, x: 60, y: 420, width: 520, size: 24)
#
# UI.clip draws what its block draws only inside a box (UI::ScrollList draws its rows in one).
module UI
  # Where each anchor is, as shares of the screen's width and height (and of the box placed there).
  ANCHORS = {
    top_left: [0.0, 0.0], top: [0.5, 0.0], top_right: [1.0, 0.0],
    left: [0.0, 0.5], center: [0.5, 0.5], right: [1.0, 0.5],
    bottom_left: [0.0, 1.0], bottom: [0.5, 1.0], bottom_right: [1.0, 1.0]
  }.freeze
  NO_ANCHOR = [0.0, 0.0].freeze      # x and y are from the top-left corner
  # Where a line sits across a text's box, as a share of the room left beside it.
  ALIGNS = { left: 0.0, center: 0.5, right: 1.0 }.freeze
  LINE_SPACING = 1.25                # from a line's top to the next one's, as a share of the size
  LINE_BREAK = "\n".freeze
  WORD_BREAK = " ".freeze            # also splits at runs of spaces and tabs (and drops them)
  KEEP_EMPTY_LINES = -1              # split's limit that keeps a text's empty last lines
  WRAPS_KEPT = 64                    # wrapped texts remembered between frames (the latest used)

  # The lines a text is drawn in at `size` when no line may be wider than `width` pixels (in the
  # face selected): broken at spaces, at the text's own line breaks, and between the letters of a
  # word too long for a line.
  def self.wrap(text, width:, size: DEFAULT_SIZE)
    key = [text.to_s, width.to_i, size.to_i, font]
    lines = wraps.delete(key) || wrapped(*key)
    wraps.shift if wraps.size >= WRAPS_KEPT
    wraps[key] = lines
  end

  # How tall a text is drawn, in pixels, from its first line's top to its last line's bottom: with
  # a width, wrapped as UI.wrap does it.
  def self.text_height(text, size: DEFAULT_SIZE, width: nil) = lines_height(text_lines(text, width, size).size, size)

  # Pixels from one line's top to the next one's.
  def self.line_height(size) = (size * LINE_SPACING).round

  # Where a box `width` by `height` starts (its top-left corner, [x, y]) when it sits at an anchor
  # by its matching corner, `x` and `y` pixels from it. Without an anchor: at [x, y].
  def self.anchored(anchor, x:, y:, width: 0, height: 0)
    across, down = anchor_shares(anchor)
    [(x + (UI.width - width) * across).to_i, (y + (UI.height - height) * down).to_i]
  end

  # Draws what the block draws only inside a box: the rest is cut off, and the pointer outside it
  # is over nothing drawn in it. Inside another clip, only where both are.
  def self.clip(x:, y:, width:, height:, anchor: nil, &)
    box = placed(anchor, x, y, width, height)
    clipping(clips.empty? ? box : overlap(box, clips.last), &)
  end

  # A box ({x:, y:, width:, height:}) placed at an anchor.
  def self.placed(anchor, x, y, width, height)
    left, top = anchored(anchor, x: x, y: y, width: width, height: height)
    { x: left, y: top, width: width, height: height }
  end

  def self.anchor_shares(anchor)
    return NO_ANCHOR if anchor.nil?

    ANCHORS.fetch(anchor) { raise ArgumentError, "unknown anchor #{anchor.inspect}" }
  end

  def self.align_share(align) = ALIGNS.fetch(align) { raise ArgumentError, "unknown align #{align.inspect}" }

  # The wrapped texts remembered, by [text, width, size, face]: the one used longest ago first.
  def self.wraps = @wraps ||= {}

  def self.wrapped(text, width, size, _face)
    paragraphs(text).flat_map { |line| Wrapping.new(width, size).lines(line) }.freeze
  end

  # The boxes UI.clip draws in now, the innermost last.
  def self.clips = @clips ||= []

  # A text's own lines: split at its line breaks (the empty ones kept).
  def self.paragraphs(text) = text.split(LINE_BREAK, KEEP_EMPTY_LINES)

  # The lines UI.text draws: wrapped to a width, or only at the text's own line breaks.
  def self.text_lines(text, width, size) = width ? wrap(text, width: width, size: size) : paragraphs(text.to_s)

  def self.lines_height(count, size) = count.positive? ? (count - 1) * line_height(size) + size.to_i : 0

  def self.widest(lines, size) = lines.map { |line| text_width(line, size: size) }.max || 0

  # How wide a text's box is: the width given, or its widest line where that is needed to place it.
  def self.text_box_width(lines, size, width, anchor, align)
    return width if width
    return 0 if from_left_edge?(anchor, align)

    widest(lines, size)
  end

  # Whether a text starts at x with nothing to measure: no anchor, and aligned left.
  def self.from_left_edge?(anchor, align) = anchor.nil? && align_share(align).zero?

  # Where a text's box starts: at an anchor by its matching corner, or across x as align says.
  def self.text_corner(box, x, y, anchor, align)
    return anchored(anchor, x: x, y: y, **box) if anchor

    [x - (box[:width] * align_share(align)).to_i, y]
  end

  # The letters of a text, each with all of its bytes.
  def self.letters_of(text)
    starts = (0...text.bytesize).reject { |index| inside_letter?(text, index) }
    starts.zip(starts.drop(1) + [text.bytesize]).map { |from, to| text.byteslice(from, to - from) }
  end

  # Draws in a box (already within any outer clip) and goes back to what was clipped before.
  def self.clipping(box)
    clips.push(box)
    begin
      __clip(*box.values_at(:x, :y, :width, :height).map(&:to_i))
      yield
    ensure
      clips.pop
      clips.empty? ? __unclip : __clip(*clips.last.values_at(:x, :y, :width, :height).map(&:to_i))
    end
  end

  # Where two boxes overlap (nothing wide, when they do not).
  def self.overlap(one, other)
    left, right = span_overlap(span(one, :x, :width), span(other, :x, :width))
    top, bottom = span_overlap(span(one, :y, :height), span(other, :y, :height))
    { x: left, y: top, width: right - left, height: bottom - top }
  end

  # A box's span along one way: [from, to].
  def self.span(box, start, length) = [box[start], box[start] + box[length]]

  # Where two spans ([from, to]) overlap: [from, to], never backwards.
  def self.span_overlap(one, other)
    start = [one.first, other.first].max
    finish = [one.last, other.last].min
    [start, [finish, start].max]
  end

  def self.pointer_in?(box) = inside?(box, Input.mouse_x, Input.mouse_y)
  def self.pointer_in_clip? = clips.empty? || pointer_in?(clips.last)
  def self.inside?(box, x, y) = within?(x, box[:x], box[:width]) && within?(y, box[:y], box[:height])

  # Whether a coordinate is on a span from `start`, `length` long (its end not on it).
  def self.within?(value, start, length) = value >= start && value < start + length

  # Lays the lines of a text out in its box: each a line height under the last, across as aligned.
  class TextBlock
    def initialize(left, top, width, size, align)
      @left = left
      @top = top
      @width = width
      @size = size
      @share = UI.align_share(align)
    end

    # Where the line at `row` starts, [x, y].
    def line_at(line, row) = [(@left + shift(line)).to_i, (@top + row * UI.line_height(@size)).to_i]

    private

    def shift(line) = @share.zero? ? 0 : ((@width - UI.text_width(line, size: @size)) * @share).to_i
  end

  # Breaks one line of a text (no line breaks of its own) into lines no wider than a width.
  class Wrapping
    def initialize(width, size)
      @width = width
      @size = size
      @lines = []
      @line = ""
    end

    def lines(paragraph)
      paragraph.split(WORD_BREAK).each { |word| add(word) }
      @lines << @line
    end

    private

    # A word goes on the line when it fits there, or starts the next one (cut, when even a line
    # of its own is too narrow for it).
    def add(word)
      longer = @line.empty? ? word : @line + WORD_BREAK + word
      return @line = longer if fits?(longer)

      next_line
      return @line = word if fits?(word)

      UI.letters_of(word).each { |letter| add_letter(letter) }
    end

    # A letter of a cut word: on the line, or starting the next (a line always takes one letter).
    def add_letter(letter)
      longer = @line + letter
      return @line = longer if @line.empty? || fits?(longer)

      @lines << @line
      @line = letter
    end

    # The line so far is done (unless it has nothing on it): the next starts empty.
    def next_line
      @lines << @line unless @line.empty?
      @line = ""
    end

    def fits?(text) = UI.text_width(text, size: @size) <= @width
  end
end
