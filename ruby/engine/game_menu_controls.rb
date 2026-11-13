# The menu's controls page, reached from its settings screen: which player's bindings it shows (a
# choice at the top), then a row for each action and each side of each axis (see
# GameMenu::ControlsPage), its keys in two cells and its gamepad buttons in two more. Up and down
# move between the rows, left and right between a row's cells; Enter, a gamepad's A or a click on a
# cell waits for the key or button that takes its place (Escape or a gamepad's Back calls that off,
# and B too for a keys' cell); a click also moves the focus there. Delete, Backspace or a gamepad's
# Y clears the focused cell. Under the rows: "Reset
# to defaults" (the player's bindings, all back to the game's), Back (to the settings, with the
# focus on their Controls button) and any option a game adds. The players' bindings are kept for
# the game's next runs as they change.
class GameMenu < Behaviour
  class ControlsScreen < Screen
    ROW_WIDTH = 840
    ROW_HEIGHT = 36
    ROW_GAP = 6
    LABEL_SHARE = 0.3         # how much of a row its name takes, on the left; the cells share the rest
    CELL_GAP = 8              # pixels between a row's name and its cells, and between the cells
    LINE_HEIGHT = 30          # pixels the column captions take above the rows, and the note under them
    CAPTION_SIZE = 20
    CAPTION_COLOR = :gold
    CELL_COLOR = :black       # a cell, on its row
    CELL_ALPHA = 140
    CONFLICT_COLOR = :orange  # the text of a cell with a binding another row has too
    CONFLICT_MARK = " !".freeze
    WAITING_MARK = "...".freeze
    CUT_MARK = "...".freeze   # ends a cell's text cut short to fit it
    VISIBLE_ROWS = 7          # rows shown at once; more scroll (UI::ScrollList), along with the focus
    PLAYER_ROWS = 1           # the player's choice, before the rows in the focus
    BUTTONS_SHARE = 0.8       # where its first button sits, as a share of the screen's height
    CELLS = ControlsPage::CELLS  # a row's cells, left to right: [column, slot]
    FIRST_CELL = 0
    # [keys, gamepad buttons] that clear the focused cell.
    CLEAR = [%i[delete backspace], %i[y]].freeze
    PROMPTS = [-> { I18n.t("game.controls.press_key") }, -> { I18n.t("game.controls.press_button") }].freeze

    # What it holds and does (the rows, the player shown, a capture waiting).
    attr_reader :page
    # The cell of the focused row that confirming and clearing work on (an index of CELLS).
    attr_reader :cell
    # Where each cell shown was drawn last: [name, side, column, slot] => {x:, y:, width:, height:}.
    attr_reader :cells

    def initialize(menu)
      leave = -> { leave(menu) }
      options = [[-> { I18n.t("game.controls.reset") }, -> { @page.reset }], [-> { I18n.t("game.menu.back") }, leave]]
      super(heading: -> { I18n.t("game.controls.heading") }, options: options, back: leave)
      @menu = menu
      @page = ControlsPage.new
      @cell = FIRST_CELL
      @list = UI::ScrollList.new
      @cells = {}
    end

    def rows = PLAYER_ROWS + @page.rows.size
    def space_above = shown_rows * row_step + 2 * @menu.scaled(LINE_HEIGHT)
    def first_button_share = BUTTONS_SHARE
    def closed = @page.cancel
    # The focused cell's column (ControlsPage::KEYS or BUTTONS) and slot.
    def column = CELLS.fetch(@cell).first
    def slot = CELLS.fetch(@cell).last

    # Shown again: from its first row, on its first keys' cell.
    def opened
      @cell = FIRST_CELL
      @list.reset
    end

    def draw(first_button)
      y = first_button[1] - space_above
      captions(y)
      y += @menu.scaled(LINE_HEIGHT)
      player_choice(row_box(y, 0))
      shared = @page.shared
      control_rows(y + row_step, shared)
      note(first_button[1] - @menu.scaled(LINE_HEIGHT), shared)
    end

    private

    # Back to the settings screen, on its Controls button (a capture still waiting is called off as
    # the page closes).
    def leave(menu)
      menu.show(:settings)
      menu.screens[:settings].focus_controls
    end

    def shown_rows = PLAYER_ROWS + [@page.rows.size, VISIBLE_ROWS].min
    def row_step = @menu.scaled(ROW_HEIGHT + ROW_GAP)
    def row_x = (UI.width - @menu.scaled(ROW_WIDTH)) / 2
    def focused?(place) = focus&.at?(place) || false
    def focus_index = focus ? focus.index : 0

    def row_box(y, place)
      { x: row_x, y: y, width: @menu.scaled(ROW_WIDTH), height: @menu.scaled(ROW_HEIGHT), focused: focused?(place) }
    end

    # The columns' names, over the cells.
    def captions(y)
      size = @menu.scaled(CAPTION_SIZE)
      [I18n.t("game.controls.keyboard"), I18n.t("game.controls.gamepad")].each_with_index do |caption, column|
        UI.text(caption, x: cell_x(column * ControlsPage::SLOTS), y: y, size: size, color: CAPTION_COLOR)
      end
    end

    def player_choice(box)
      numbers = (1..Input::PLAYERS).map(&:to_s)
      picked = UI.choice(I18n.t("game.controls.player"), numbers, @page.player, **box)
      @page.player = picked unless picked == @page.player
    end

    # The rows the focus is among, in a list that scrolls to the focused one (and with the wheel,
    # but not while a capture waits for it); each frame's left or right first picks the focused
    # row's cell. `shared`: the bindings on more than one row.
    def control_rows(y, shared)
      all = @page.rows
      pick_cell if focus_on_a_row?
      @cells = {}
      @list.draw(all.size, **list_box(y), focus: focus, first: PLAYER_ROWS, input: !@page.capturing?) do |index, box|
        place = PLAYER_ROWS + index
        control_row(all[index], box.merge(focused: focused?(place)), place, shared)
      end
    end

    # Where the rows' list is, under the player's choice: as tall as the rows it shows at once.
    def list_box(y)
      { x: row_x, y: y, width: @menu.scaled(ROW_WIDTH), height: (shown_rows - PLAYER_ROWS) * row_step,
        row_height: row_step, gap: row_step - @menu.scaled(ROW_HEIGHT) }
    end

    # Whether the focus is on one of the rows (not on the player's choice, nor on a button).
    def focus_on_a_row? = (focus_index - PLAYER_ROWS).between?(0, @page.rows.size - 1)
    def pick_cell = @cell = (@cell + UI.sideways).clamp(FIRST_CELL, CELLS.size - 1)

    # A row at `place` in the focus.
    def control_row(row, box, place, shared)
      focused = box.delete(:focused)
      UI.row(BindingNames.row(row.name, row.side), nil, box, false)
      CELLS.each_index do |index|
        control_cell(row, [place, index], cell_in(box, index), focused && index == @cell, shared)
      end
    end

    # A cell (`spot`: its row's place in the focus, and its index in CELLS): ringed and lit when it
    # is the focused one; worked (clicked, or confirmed while focused), it waits for what takes its
    # place, unless a capture already waits, and takes the focus. Focused, it may be cleared.
    def control_cell(row, spot, box, focused, shared)
      column, slot = CELLS.fetch(spot.last)
      @cells[[row.name, row.side, column, slot]] = box
      draw_cell(box, focused)
      cell_text(row, [column, slot], box, @page.conflict?(row, column, slot, shared))
      rebind(row, spot) if rebinding?(box, focused)
      @page.clear(row, column, slot) if clearing?(focused)
    end

    def draw_cell(box, focused)
      UI.focus_ring(box) if focused
      fill, alpha = focused ? [UI::BUTTON_HOVER_COLOR, UI::BUTTON_ALPHA] : [CELL_COLOR, CELL_ALPHA]
      UI.rect(**box, color: fill, alpha: alpha, radius: UI::BUTTON_RADIUS)
    end

    # Waits for a cell's binding, with the focus moved to it (it was clicked, maybe).
    def rebind(row, spot)
      place, @cell = spot
      focus&.move_to(place)
      @page.capture(row, column, slot)
    end

    # Whether a cell is worked this frame to wait for its binding (not while one is waited for).
    def rebinding?(box, focused) = !@page.capturing? && UI.worked?(box, focused)

    # Whether the focused cell is cleared this frame (not while a capture waits: what it catches is
    # its own).
    def clearing?(focused) = focused && !@page.capturing? && UI.pressed_any?(CLEAR)

    # A cell's text (`at`: its column and slot).
    def cell_text(row, at, box, conflict)
      size = UI.label_size(box[:height])
      text = waiting?(row, at) ? WAITING_MARK : @page.text(row, *at)
      text = fitted(text, box[:width] - 2 * UI::FIELD_PADDING, size, conflict ? CONFLICT_MARK : "")
      UI.text(text, x: box[:x] + UI::FIELD_PADDING, y: UI.centered(box[:y], box[:height], size), size: size,
                    color: conflict ? CONFLICT_COLOR : :white)
    end

    # A text with `mark` after it, cut short (and ending in CUT_MARK) where it would be wider than
    # `width` at `size`: the mark always shows.
    def fitted(text, width, size, mark)
      return text + mark if fits?(text + mark, width, size)

      text = UI.chop(text) until cut_enough?(text, width, size, mark)
      text + CUT_MARK + mark
    end

    def fits?(text, width, size) = UI.text_width(text, size: size) <= width

    # Whether a text being cut short fits with its marks (or has nothing more to cut).
    def cut_enough?(text, width, size, mark) = text.empty? || fits?(text + CUT_MARK + mark, width, size)

    def waiting?(row, at) = @page.waiting == [row, *at]

    # Where a cell (an index of CELLS) starts, and a cell of a row's box.
    def cell_x(index) = row_x + label_width + (index + 1) * @menu.scaled(CELL_GAP) + index * cell_width
    def label_width = (@menu.scaled(ROW_WIDTH) * LABEL_SHARE).to_i
    def cell_width = (@menu.scaled(ROW_WIDTH) - label_width - CELLS.size * @menu.scaled(CELL_GAP)) / CELLS.size

    def cell_in(row_box, index) = { x: cell_x(index), y: row_box[:y], width: cell_width, height: row_box[:height] }

    # Under the rows: what to press while a capture waits, what the marks mean when one shows, or
    # else how to clear a cell.
    def note(y, shared)
      text = note_text(shared)
      UI.text(text, x: UI.width / 2, y: y, size: @menu.scaled(CAPTION_SIZE), align: :center) if text
    end

    def note_text(shared)
      return PROMPTS.fetch(@page.waiting_column).call if @page.capturing?

      shared.any? ? I18n.t("game.controls.conflict_note") : I18n.t("game.controls.clear_note")
    end
  end
end
