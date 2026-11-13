# What the menu's controls page holds and does, apart from how it is drawn (see
# GameMenu::ControlsScreen): a row for each of the game's actions and for each side of each of its
# axes, for one player at a time. Each row has two columns, the player's keys and mouse buttons, and
# their gamepad buttons, and each column SLOTS cells: its first binding, its second (W and Up), the
# last cell showing any more a game bound. Rebinding a cell waits for the next key, mouse button or
# wheel turn (a keys' cell) or gamepad button (a buttons' cell) pressed, and that one takes the
# place of what the cell had; the row's other cells keep their own, in their places. Clearing a
# cell takes its bindings away. The click that asked for the wait is not taken for the binding, and
# the one caught clicks nothing on the page; a click while a buttons' cell waits works the page as
# usual.
# A key or button bound to two rows is allowed (both see it), and is marked; caught for a row's
# column that already has it, it changes nothing (a binding is never listed twice). A buttons' cell
# takes a button from the shown player's own gamepad only. What it shows is worked out again only
# after the bindings change (Input.bindings_changes) or another player is shown.
class GameMenu < Behaviour
  class ControlsPage
    KEYS = 0       # the cell of a row's keys and mouse buttons
    BUTTONS = 1    # the cell of its gamepad buttons
    SLOTS = 2      # cells a column has on each row: the first binding, then the second and any more
    FIRST_SLOT = 0
    WAITING_COLUMN = 1  # where `waiting` ([row, column, slot]) holds its column
    # A row's cells, left to right: [column, slot].
    CELLS = [KEYS, BUTTONS].flat_map { |column| (FIRST_SLOT...SLOTS).map { |slot| [column, slot].freeze } }.freeze
    # What the bindings of each cell start with.
    PREFIXES = [[Input::KEY_PREFIX, Input::MOUSE_PREFIX].freeze, [Input::PAD_PREFIX].freeze].freeze
    SIDES = %i[negative positive].freeze
    SEPARATOR = ", ".freeze  # between the names of a cell's bindings
    # What calls a wait off, for each cell: Escape, and a gamepad's B or Back for the keys' cell;
    # only Escape and Back for the buttons' cell, so that B can be bound there.
    CANCELS = [Input::CANCEL_CAPTURE, ["key:escape", "pad:back"]].freeze

    # An action (side nil), or one side of an axis.
    Row = Struct.new(:name, :side)

    # The player whose bindings are shown (a gamepad's number: 0 also has the keyboard).
    attr_reader :player

    def initialize
      @player = Input::FIRST_PAD
      @waiting = nil
      @capture = ->(binding, pad) { caught(binding, pad) }
      @seen = nil
    end

    # The cell a capture is waiting to fill, [row, column, slot]; nil while none is (also once something
    # else called the capture off, or asked for one of its own).
    def waiting = capturing? ? @waiting : nil

    # The column of the cell a capture is waiting to fill (KEYS or BUTTONS; nil while none is).
    def waiting_column = waiting&.fetch(WAITING_COLUMN)

    # Every action of the game, then both sides of every axis.
    def rows
      refresh
      @rows ||= Input.actions.map { |name| Row.new(name, nil) } +
                Input.axes.flat_map { |name| SIDES.map { |side| Row.new(name, side) } }
    end

    # Shows another player's bindings (a capture waiting for the last one is called off).
    def player=(pad)
      cancel
      @player = pad
    end

    # The player's bindings of a cell, as text ("key:space", "mouse:left").
    def bindings(row, column)
      refresh
      @cells[[row.name, row.side, column]] ||= of_row(row).select { |binding| of_column?(binding, column) }
    end

    # The player's bindings a cell shows: its slot's one, or (the column's last slot) every one
    # from there on.
    def slot_bindings(row, column, slot)
      all = bindings(row, column)
      last_slot?(slot) ? all.drop(slot) : Array(all[slot])
    end

    # What a cell shows (or, with no slot, a whole column): its bindings by name, or "None". An
    # axis' stick is in its first buttons' cell, though it is the game's (no player rebinds it).
    def text(row, column, slot = nil)
      names = shown(row, column, slot).map { |binding| BindingNames.of(binding) }
      names << BindingNames.stick(stick_of(row)) if stick_shown?(row, column, slot)
      names.empty? ? I18n.t("game.controls.none") : names.join(SEPARATOR)
    end

    # The bindings the player has on more than one row.
    def shared
      refresh
      @shared ||= counted.select { |_binding, rows| rows > 1 }.keys
    end

    # Whether a cell (or a whole column, with no slot) has a binding another row has too
    # (`shared`, worked out once for many cells).
    def conflict?(row, column, slot = nil, shared = self.shared)
      shown(row, column, slot).any? { |binding| shared.include?(binding) }
    end

    def capturing? = !@waiting.nil? && Input.capturing_for?(@capture)

    # Waits for the key or button that will fill a cell (the mouse only for the keys' cells).
    def capture(row, column, slot = FIRST_SLOT)
      @waiting = [row, column, slot]
      Input.capture_next(cancel: CANCELS.fetch(column), mouse: column == KEYS, &@capture)
    end

    # Stops waiting: the cell keeps what it had.
    def cancel
      Input.cancel_capture if capturing?
      @waiting = nil
    end

    # Takes a cell's bindings away (a capture still waiting is called off first).
    def clear(row, column, slot)
      cancel
      unbind_all(row, slot_bindings(row, column, slot))
    end

    # The player's bindings, all of them, back to the game's.
    def reset
      cancel
      Input.reset_bindings(pad: @player)
    end

    # What a capture caught (and on which gamepad: nil for a key): nothing (it was called off)
    # leaves the cell as it was; a binding of the other kind (a button, for the keys' cell), or a
    # button of another player's gamepad, is not for it: it waits on.
    def caught(binding, pad = nil)
      return @waiting = nil if binding.nil?
      return capture(*@waiting) unless fits?(binding, pad, @waiting.fetch(WAITING_COLUMN))

      replace(*@waiting, binding)
      @waiting = nil
    end

    private

    # Whether a binding caught is one the cell takes: of its kind, and a button from the shown
    # player's own gamepad.
    def fits?(binding, pad, column)
      return false unless of_column?(binding, column)

      column == KEYS || pad == @player
    end

    def of_column?(binding, column) = binding.start_with?(*PREFIXES.fetch(column))

    # Forgets what it worked out when the bindings changed, or another player is shown.
    def refresh
      seen = [Input.bindings_changes, @player]
      return if seen == @seen

      @seen = seen
      @rows = @shared = nil
      @cells = {}
    end

    # All the player's bindings of a row (an action's, or one side of an axis').
    def of_row(row)
      all = Input.bindings(row.name, pad: @player)
      row.side ? all.fetch(row.side) : all
    end

    # How many rows each of the player's bindings is on.
    def counted
      seen = Hash.new(0)
      rows.each { |row| row_bindings(row).each { |binding| seen[binding] += 1 } }
      seen
    end

    # A slot's bindings give way to the one caught, the column's others keeping their places (keys
    # still list before mouse buttons, so a mouse button caught for a first cell shows after the
    # keys). One the column already has changes nothing.
    def replace(row, column, slot, binding)
      now = bindings(row, column)
      return if now.include?(binding)

      unbind_all(row, now)
      put(now, slot, binding).each { |one| Input.bind(row.name, one, side: row.side, pad: @player) }
    end

    # A column's bindings with a slot's taken by `binding` (a slot past the last binding: added at
    # the end).
    def put(now, slot, binding)
      wanted = now.dup
      wanted[[slot, now.size].min, last_slot?(slot) ? now.size : 1] = [binding]
      wanted
    end

    def unbind_all(row, bindings)
      bindings.each { |old| Input.unbind(row.name, old, side: row.side, pad: @player) }
    end

    def shown(row, column, slot) = slot.nil? ? bindings(row, column) : slot_bindings(row, column, slot)
    def last_slot?(slot) = slot == SLOTS - 1
    def row_bindings(row) = bindings(row, KEYS) + bindings(row, BUTTONS)
    def stick_of(row) = row.side && Input.axis_stick(row.name)

    # Whether a cell shows the row's stick: an axis' first buttons' cell, or its whole buttons' column.
    def stick_shown?(row, column, slot) = column == BUTTONS && first_or_whole?(slot) && !stick_of(row).nil?
    def first_or_whole?(slot) = slot.nil? || slot == FIRST_SLOT
  end
end
