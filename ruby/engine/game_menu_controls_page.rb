# What the menu's controls page holds and does, apart from how it is drawn (see
# GameMenu::ControlsScreen): a row for each of the game's actions and for each side of each of its
# axes, for one player at a time. Each row has two cells, the player's keys and their gamepad
# buttons. Rebinding a cell waits for the next key (the keys' cell) or button (the buttons' cell)
# pressed, and that one takes the place of what the cell had; the row's other cell keeps its own.
# A key or button bound to two rows is allowed (both see it), and is marked.
class GameMenu < Behaviour
  class ControlsPage
    KEYS = 0       # the cell of a row's keys
    BUTTONS = 1    # the cell of its gamepad buttons
    # What the bindings of each cell start with.
    PREFIXES = [Input::KEY_PREFIX, Input::PAD_PREFIX].freeze
    SIDES = %i[negative positive].freeze
    SEPARATOR = ", ".freeze  # between the names of a cell's bindings
    # What calls a wait off, for each cell: Escape, and a gamepad's B or Back for the keys' cell;
    # only Escape and Back for the buttons' cell, so that B can be bound there.
    CANCELS = [Input::CANCEL_CAPTURE, ["key:escape", "pad:back"]].freeze

    # An action (side nil), or one side of an axis.
    Row = Struct.new(:name, :side)

    # The player whose bindings are shown (a gamepad's number: 0 also has the keyboard).
    attr_reader :player
    # The cell a capture is waiting to fill, [row, column]; nil while none is.
    attr_reader :waiting

    def initialize
      @player = Input::FIRST_PAD
      @waiting = nil
    end

    # Every action of the game, then both sides of every axis.
    def rows
      actions = Input.actions.map { |name| Row.new(name, nil) }
      actions + Input.axes.flat_map { |name| SIDES.map { |side| Row.new(name, side) } }
    end

    # Shows another player's bindings (a capture waiting for the last one is called off).
    def player=(pad)
      cancel
      @player = pad
    end

    # The player's bindings of a cell, as text ("key:space").
    def bindings(row, column)
      all = Input.bindings(row.name, pad: @player)
      all = all.fetch(row.side) if row.side
      all.select { |binding| binding.start_with?(PREFIXES.fetch(column)) }
    end

    # What a cell shows: its bindings by name, or "None". An axis' stick is among its buttons,
    # though it is the game's (no player rebinds it).
    def text(row, column)
      names = bindings(row, column).map { |binding| BindingNames.of(binding) }
      names << BindingNames.stick(stick_of(row)) if stick_shown?(row, column)
      names.empty? ? I18n.t("game.controls.none") : names.join(SEPARATOR)
    end

    # The bindings the player has on more than one row.
    def shared
      seen = Hash.new(0)
      rows.each { |row| row_bindings(row).each { |binding| seen[binding] += 1 } }
      seen.select { |_binding, rows| rows > 1 }.keys
    end

    # Whether a cell has a binding another row has too (`shared`, worked out once for many cells).
    def conflict?(row, column, shared = self.shared) = bindings(row, column).any? { |binding| shared.include?(binding) }

    def capturing? = !@waiting.nil?

    # Waits for the key or button that will fill a cell.
    def capture(row, column)
      @waiting = [row, column]
      Input.capture_next(cancel: CANCELS.fetch(column)) { |binding, _pad| caught(binding) }
    end

    # Stops waiting: the cell keeps what it had.
    def cancel
      Input.cancel_capture if capturing?
      @waiting = nil
    end

    # The player's bindings, all of them, back to the game's.
    def reset
      cancel
      Input.reset_bindings(pad: @player)
    end

    # What a capture caught: nothing (it was called off) leaves the cell as it was, and a binding
    # of the other kind (a button, for the keys' cell) is not for it: it waits on.
    def caught(binding)
      return @waiting = nil if binding.nil?
      return capture(*@waiting) unless fits?(binding, @waiting.last)

      replace(*@waiting, binding)
      @waiting = nil
    end

    private

    def fits?(binding, column) = binding.start_with?(PREFIXES.fetch(column))

    # The cell's bindings give way to the one caught.
    def replace(row, column, binding)
      bindings(row, column).each { |old| Input.unbind(row.name, old, side: row.side, pad: @player) }
      Input.bind(row.name, binding, side: row.side, pad: @player)
    end

    def row_bindings(row) = bindings(row, KEYS) + bindings(row, BUTTONS)
    def stick_of(row) = row.side && Input.axis_stick(row.name)
    def stick_shown?(row, column) = column == BUTTONS && !stick_of(row).nil?
  end
end
