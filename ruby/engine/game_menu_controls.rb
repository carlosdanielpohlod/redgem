# The menu's controls page, reached from its settings screen: which player's bindings it shows (a
# choice at the top), then a row for each action and each side of each axis (see
# GameMenu::ControlsPage), the keys in one cell and the gamepad buttons in the other. Up and down
# move between the rows, left and right between a row's two cells; Enter, a gamepad's A or a click
# on a cell waits for the key or button that takes its place (Escape, or a gamepad's B or Back,
# calls that off). Under the rows: "Reset to defaults" (the player's bindings, all back to the
# game's) and Back. The players' bindings are kept for the game's next runs as they change.
class GameMenu < Behaviour
  class ControlsScreen < Screen
    ROW_WIDTH = 720
    ROW_HEIGHT = 36
    ROW_GAP = 6
    LABEL_SHARE = 0.34        # how much of a row its name takes, on the left; the cells share the rest
    CELL_GAP = 8              # pixels between a row's name and its cells, and between the cells
    LINE_HEIGHT = 30          # pixels the column captions take above the rows, and the note under them
    CAPTION_SIZE = 20
    CAPTION_COLOR = :gold
    CELL_COLOR = :black       # a cell, on its row
    CELL_ALPHA = 140
    CONFLICT_COLOR = :orange  # the text of a cell with a binding another row has too
    CONFLICT_MARK = " !".freeze
    WAITING_MARK = "...".freeze
    VISIBLE_ROWS = 7          # rows shown at once; more scroll along with the focus
    PLAYER_ROWS = 1           # the player's choice, before the rows in the focus
    BUTTONS_SHARE = 0.8       # where its first button sits, as a share of the screen's height
    # Which cell left and right go to.
    SIDEWAYS_COLUMNS = { -1 => ControlsPage::KEYS, 1 => ControlsPage::BUTTONS }.freeze
    PROMPTS = [-> { I18n.t("game.controls.press_key") }, -> { I18n.t("game.controls.press_button") }].freeze

    # What it holds and does (the rows, the player shown, a capture waiting).
    attr_reader :page
    # The cell of the focused row that confirming works on (ControlsPage::KEYS or BUTTONS).
    attr_reader :column
    # Where each cell shown was drawn last: [name, side, column] => {x:, y:, width:, height:}.
    attr_reader :cells

    def initialize(menu)
      super(heading: -> { I18n.t("game.controls.heading") }, back: -> { leave(menu) })
      @menu = menu
      @page = ControlsPage.new
      @column = ControlsPage::KEYS
      @top = 0
      @cells = {}
    end

    def options = [[I18n.t("game.controls.reset"), -> { @page.reset }], [I18n.t("game.menu.back"), back]]
    def rows = PLAYER_ROWS + @page.rows.size
    def space_above = shown_rows * row_step + 2 * @menu.scaled(LINE_HEIGHT)
    def first_button_share = BUTTONS_SHARE
    def closed = @page.cancel

    # Shown again: from its first row, on the keys' cells.
    def opened
      @column = ControlsPage::KEYS
      @top = 0
    end

    def draw(first_button)
      y = first_button[1] - space_above
      captions(y)
      y += @menu.scaled(LINE_HEIGHT)
      player_choice(row_box(y, 0))
      control_rows(y + row_step)
      note(first_button[1] - @menu.scaled(LINE_HEIGHT))
    end

    private

    # Back to the settings screen; a capture still waiting is called off.
    def leave(menu)
      @page.cancel
      menu.show(:settings)
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
        UI.text(caption, x: cell_x(column), y: y, size: size, color: CAPTION_COLOR)
      end
    end

    def player_choice(box)
      numbers = (1..Input::PLAYERS).map(&:to_s)
      picked = UI.choice(I18n.t("game.controls.player"), numbers, @page.player, **box)
      @page.player = picked unless picked == @page.player
    end

    # The rows the focus is among, scrolled so the focused one shows; each frame's left or right
    # first picks the focused row's cell.
    def control_rows(y)
      all = @page.rows
      scroll_to(focus_index - PLAYER_ROWS, all.size)
      pick_column if focus_on_a_row?
      shared = @page.shared
      @cells = {}
      all[@top, VISIBLE_ROWS].each_with_index do |row, at|
        control_row(row, row_box(y + at * row_step, PLAYER_ROWS + @top + at), shared)
      end
    end

    # Keeps the row at `index` among those shown (one before the first is the player's choice).
    def scroll_to(index, count)
      @top = index if index.between?(0, @top - 1)
      @top = index - VISIBLE_ROWS + 1 if index >= @top + VISIBLE_ROWS
      @top = @top.clamp(0, [count - VISIBLE_ROWS, 0].max)
    end

    def focus_on_a_row? = focus_index >= PLAYER_ROWS
    def pick_column = @column = SIDEWAYS_COLUMNS.fetch(UI.sideways, @column)

    def control_row(row, box, shared)
      focused = box.delete(:focused)
      UI.row(BindingNames.row(row.name, row.side), nil, box, false)
      [ControlsPage::KEYS, ControlsPage::BUTTONS].each do |column|
        cell(row, column, cell_in(box, column), focused && column == @column, shared)
      end
    end

    # A cell: ringed and lit when it is the focused one; worked (clicked, or confirmed while
    # focused), it waits for what takes its place, unless a capture already waits.
    def cell(row, column, box, focused, shared)
      @cells[[row.name, row.side, column]] = box
      UI.focus_ring(box) if focused
      fill, alpha = focused ? [UI::BUTTON_HOVER_COLOR, UI::BUTTON_ALPHA] : [CELL_COLOR, CELL_ALPHA]
      UI.rect(**box, color: fill, alpha: alpha, radius: UI::BUTTON_RADIUS)
      cell_text(row, column, box, @page.conflict?(row, column, shared))
      @page.capture(row, column) if rebinding?(box, focused)
    end

    # Whether a cell is worked this frame to wait for its binding (not while one is waited for).
    def rebinding?(box, focused) = !@page.capturing? && UI.worked?(box, focused)

    def cell_text(row, column, box, conflict)
      size = UI.label_size(box[:height])
      text = waiting?(row, column) ? WAITING_MARK : @page.text(row, column)
      text += CONFLICT_MARK if conflict
      UI.text(text, x: box[:x] + UI::FIELD_PADDING, y: UI.centered(box[:y], box[:height], size), size: size,
                    color: conflict ? CONFLICT_COLOR : :white)
    end

    def waiting?(row, column) = @page.waiting == [row, column]

    # Where a column's cells start, and a cell of a row's box.
    def cell_x(column) = row_x + label_width + (column + 1) * @menu.scaled(CELL_GAP) + column * cell_width
    def label_width = (@menu.scaled(ROW_WIDTH) * LABEL_SHARE).to_i
    def cell_width = (@menu.scaled(ROW_WIDTH) - label_width - 2 * @menu.scaled(CELL_GAP)) / 2

    def cell_in(row_box, column) = { x: cell_x(column), y: row_box[:y], width: cell_width, height: row_box[:height] }

    # Under the rows: what to press while a capture waits, or what the marks mean when one shows.
    def note(y)
      text = note_text
      UI.text(text, x: UI.width / 2, y: y, size: @menu.scaled(CAPTION_SIZE), align: :center) if text
    end

    def note_text
      return PROMPTS.fetch(@page.waiting.last).call if @page.capturing?

      I18n.t("game.controls.conflict_note") if @page.shared.any?
    end
  end
end
