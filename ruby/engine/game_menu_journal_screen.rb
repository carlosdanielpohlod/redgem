# The menu's journal page (see GameMenu::Journal for what it lists): the quests under way, with the
# stage each is at and its objectives (a filled box: done), then the quests completed. Its lines
# scroll (UI::ScrollList) and take the focus: up and down move between them (arrows, a gamepad's
# D-pad), Page Up and Page Down (the bumpers), the wheel and the right stick scroll. Picking a line
# of a quest under way (Enter, A, a click or a tap) tracks that quest: this machine's markers show
# its places (see QuestMarkers). Its Back button, Esc or a gamepad's Start go back to the pause
# menu. The pause menu of a game with quest files offers it (GameMenu's setting journal: false
# leaves it out).
class GameMenu < Behaviour
  class JournalScreen < Screen
    ROW_WIDTH = 640
    ROW_HEIGHT = 30
    ROW_GAP = 4
    VISIBLE_ROWS = 11          # lines shown at once; more scroll
    TEXT_SIZE = 20
    INDENT = 24                # units a stage's text and an objective sit right of a quest's title
    BOX_SIZE = 14              # units across an objective's box
    BOX_BORDER = 2
    BUTTONS_SHARE = 0.86       # where its Back button sits, as a share of the screen's height
    DONE_COLOR = :green
    TO_DO_COLOR = :gray
    # How each kind of line is drawn: its text's color, and whether it sits on a bar as a button does.
    COLORS = { section: :gold, quest: :white, stage: :white, objective: :white, none: :gray, completed: :gray }.freeze
    BARS = %i[quest completed].freeze
    INDENTED = %i[stage objective].freeze

    def initialize(menu)
      back = -> { menu.show(:pause) }
      super(heading: -> { I18n.t("game.journal.heading") }, options: [[-> { I18n.t("game.menu.back") }, back]],
            back: back)
      @menu = menu
      @list = UI::ScrollList.new
    end

    # What it shows now (GameMenu::Journal::Line each).
    def lines = Journal.lines

    def rows = lines.size
    def space_above = VISIBLE_ROWS * row_step
    def first_button_share = BUTTONS_SHARE
    def opened = @list.reset

    def draw(first_button)
      shown = lines
      @list.draw(shown.size, **list_box(first_button[1] - space_above), focus: focus) do |index, box|
        line(shown[index], box, focus&.at?(index) || false)
      end
    end

    private

    def row_step = @menu.scaled(ROW_HEIGHT + ROW_GAP)
    def row_x = (UI.width - @menu.scaled(ROW_WIDTH)) / 2

    def list_box(y)
      { x: row_x, y: y, width: @menu.scaled(ROW_WIDTH), height: VISIBLE_ROWS * row_step, row_height: row_step,
        gap: row_step - @menu.scaled(ROW_HEIGHT) }
    end

    # A line, ringed while it has the focus; worked, it tracks its quest.
    def line(line, box, focused)
      UI.focus_ring(box) if focused
      bar(box) if focused || BARS.include?(line.kind)   # (the ring shows around a bar)
      x = text_x(line, box)
      size = @menu.scaled(TEXT_SIZE)
      UI.text(line.text, x: x, y: UI.centered(box[:y], box[:height], size), size: size, color: color_of(line))
      Quests.track(line.quest.name) if tracking?(line, box, focused)
    end

    def bar(box) = UI.rect(**box, color: UI::BUTTON_COLOR, alpha: UI::BUTTON_ALPHA, radius: UI::BUTTON_RADIUS)

    # Where a line's text starts: indented under its quest's title, past an objective's box.
    def text_x(line, box)
      x = box[:x] + UI::FIELD_PADDING + (INDENTED.include?(line.kind) ? @menu.scaled(INDENT) : 0)
      line.kind == :objective ? objective_box(x, box, line.done) : x
    end

    # An objective's box, filled once it is done. Returns where its text starts.
    def objective_box(x, row, done)
      size = @menu.scaled(BOX_SIZE)
      y = UI.centered(row[:y], row[:height], size)
      UI.rect(x: x, y: y, width: size, height: size, color: done ? DONE_COLOR : TO_DO_COLOR)
      inner = size - 2 * BOX_BORDER
      UI.rect(x: x + BOX_BORDER, y: y + BOX_BORDER, width: inner, height: inner, color: :dark) unless done
      x + size + @menu.scaled(ROW_GAP) * 2
    end

    # (An objective done is grayed.)
    def color_of(line) = line.kind == :objective && line.done ? TO_DO_COLOR : COLORS.fetch(line.kind)

    # Whether a line of a quest under way is worked this frame (clicked, tapped, or picked while focused).
    def tracking?(line, box, focused) = trackable?(line) && UI.worked?(box, focused)

    def trackable?(line) = !line.quest.nil? && line.quest.active?
  end
end
