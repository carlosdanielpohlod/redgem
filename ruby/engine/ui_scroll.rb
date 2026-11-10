# A list taller than the room it has: its rows are drawn in a box, cut off at its edges, and it
# scrolls. Like the rest of UI it is immediate (draw it every frame), but it keeps how far it is
# scrolled, so make it once:
#
#   @list ||= UI::ScrollList.new
#   @focus ||= UI::Focus.new
#   @focus.update(@items.size)
#   @list.draw(@items.size, x: 40, y: 120, width: 480, height: 300, row_height: 52, gap: 8,
#              focus: @focus) do |index, box|
#     pick(@items[index]) if UI.button(@items[index], **box, focused: @focus.at?(index))
#   end
#
# The mouse wheel scrolls it while the pointer is over it, and so does a gamepad's right stick;
# Page Up and Page Down (a gamepad's bumpers) go a box's height of rows at a time. Given a focus
# (UI::Focus), the list scrolls to the row the focus moves to (row 0 is the focus' `first`, so the
# rows may come after other widgets), and Page Up and Page Down move the focus. Without one, up
# and down (arrows, D-pad) scroll a row. A bar beside its right edge shows how far it is scrolled.
# Every list drawn hears these keys and the stick: draw it with `input: false` while the game uses
# them for something else (walking with the arrows, turning the camera with the right stick).
module UI
  PAGE_UP = [%i[page_up], %i[left_bumper]].freeze
  PAGE_DOWN = [%i[page_down], %i[right_bumper]].freeze
  SCROLL_STICK = :right_y
  STICK_DEAD_ZONE = 0.25         # a stick pushed less than this scrolls nothing
  STICK_ROWS_PER_SECOND = 8.0    # rows a stick pushed all the way scrolls in a second
  WHEEL_ROWS = 1                 # rows a turn of the mouse wheel scrolls
  SCROLLBAR_WIDTH = 6
  SCROLLBAR_GAP = 6              # pixels between a list and its bar
  SCROLLBAR_MIN = 16             # the shortest a bar's handle gets
  SCROLLBAR_COLOR = :gray
  SCROLLBAR_TRACK_ALPHA = 70
  SCROLLBAR_HANDLE_ALPHA = 200

  # Rows in a box that scrolls (see above).
  class ScrollList
    # How far it is scrolled, in pixels from its first row's top (kept within its rows each draw).
    attr_accessor :offset

    def initialize
      @offset = 0
      @followed = nil
      lay_out(0, 1, { x: 0, y: 0, width: 0, height: 0 }) # nothing to show before it is drawn
    end

    # Back to its first row.
    def reset
      @offset = 0
      @followed = nil
    end

    # Draws a list of `count` rows, each `row_height` pixels from the last's top (the last `gap` of
    # it left empty), in a box (placed at an anchor, when given one): calls the block with the
    # index and the box ({x:, y:, width:, height:}) of every row that shows, and only inside the
    # box. `input: false` leaves the wheel, the stick and the keys to something else this frame.
    def draw(count, x:, y:, width:, height:, row_height:, gap: 0, focus: nil, first: 0, anchor: nil, input: true)
      lay_out(count, row_height, UI.placed(anchor, x, y, width, height))
      @focus = focus
      @first = first
      hear if input
      follow
      UI.clip(**clip_box(gap)) { shown.each { |index| yield index, row_box(index, gap) } }
      scrollbar if scrolls?
    end

    # The rows that show (some of them maybe only in part), by index.
    def shown
      top = (@offset / @row_height).floor
      top..[((@offset + @box[:height]).to_f / @row_height).ceil - 1, @count - 1].min
    end

    # Whether its rows are taller than its box.
    def scrolls? = most.positive?

    private

    def lay_out(count, row_height, box)
      raise ArgumentError, "a scroll list's row_height must be positive" unless row_height.positive?

      @count = count
      @row_height = row_height
      @box = box
      keep_in
    end

    # The farthest it scrolls: its last row's bottom at the box's.
    def most = [@count * @row_height - @box[:height], 0].max
    def keep_in = @offset = @offset.clamp(0, most)

    def scroll(pixels)
      @offset += pixels
      keep_in
    end

    # This frame's wheel, stick and keys.
    def hear
      pages = UI.direction(PAGE_UP, PAGE_DOWN)
      return focus_by(pages * page_rows) if paging_focus?(pages)

      rows = pages * page_rows + (@focus ? 0 : UI.direction(UP, DOWN))
      scroll(rows * @row_height + wheel_pixels + stick_pixels)
    end

    # Whether a page key moves the focus: it was pressed with the focus on one of the rows.
    def paging_focus?(pages) = !pages.zero? && !focused_row.nil?

    # The rows a page goes: as many as show whole (at least one).
    def page_rows = [@box[:height] / @row_height, 1].max.to_i

    # (Only with the pointer over it where it shows: inside any clip it is drawn in.)
    def wheel_pixels = UI.mouse_over?(**@box) ? -Input.mouse_wheel * WHEEL_ROWS * @row_height : 0

    def stick_pixels
      push = Input.gamepad_axis(SCROLL_STICK)
      push.abs < STICK_DEAD_ZONE ? 0 : push * STICK_ROWS_PER_SECOND * @row_height * Time.unscaled_delta
    end

    # The row the focus is on, if it is on one of the list's.
    def focused_row
      row = @focus && @focus.index - @first
      row if row&.between?(0, @count - 1)
    end

    # Moves the focus a number of rows (staying on the list's).
    def focus_by(rows) = @focus.move_to(@first + (focused_row + rows).clamp(0, @count - 1))

    # When the focus has moved to another row, or the rows or the box changed size (another
    # resolution), scrolls so that the focused row shows whole.
    def follow
      row = focused_row
      seen = [row, @row_height, @box[:height]]
      show_row(row) if row && seen != @followed
      @followed = seen
    end

    def show_row(row)
      top = row * @row_height
      @offset = top if top < @offset
      @offset = top + @row_height - @box[:height] if top + @row_height > @offset + @box[:height]
      keep_in
    end

    def row_box(index, gap)
      { x: @box[:x], y: (@box[:y] + index * @row_height - @offset).to_i, width: @box[:width],
        height: @row_height - gap }
    end

    # Where rows are drawn: the box, with room beside it for a focused row's ring, and above it as
    # much as the gap a row leaves under it (the ring of the top row; the next row's top stays
    # hidden below, where the gap of the last one shown already makes room).
    def clip_box(gap)
      above = [FOCUS_RING, gap].min
      { x: @box[:x] - FOCUS_RING, y: @box[:y] - above, width: @box[:width] + 2 * FOCUS_RING,
        height: @box[:height] + above }
    end

    # Beside its right edge: a track as tall as the box, and a handle as far down it as it is scrolled.
    def scrollbar
      track = { x: @box[:x] + @box[:width] + SCROLLBAR_GAP, y: @box[:y], width: SCROLLBAR_WIDTH, height: @box[:height] }
      bar(track, SCROLLBAR_TRACK_ALPHA)
      handle = handle_height
      bar(track.merge(y: track[:y] + ((@box[:height] - handle) * @offset / most).to_i, height: handle),
          SCROLLBAR_HANDLE_ALPHA)
    end

    def bar(box, alpha) = UI.rect(**box, color: SCROLLBAR_COLOR, alpha: alpha, radius: SCROLLBAR_WIDTH / 2)

    # The handle's share of the track is the box's of the rows (never shorter than SCROLLBAR_MIN,
    # nor than the track).
    def handle_height
      share = [@box[:height] * @box[:height] / (@count * @row_height), SCROLLBAR_MIN].max
      [share, @box[:height]].min.to_i
    end
  end
end
