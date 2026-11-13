# A game's actions and axes, for each player. A player is a gamepad's number: 0 is the first (who
# also has the keyboard and the mouse), up to 3 (Input::PLAYERS players, as the engine counts
# them); pad: nil is the first too. Bindings are written as text ("key:space" is a key,
# "mouse:left" a mouse button, "mouse:wheel_up" the wheel turned up, "pad:a" a gamepad button), as
# a Symbol (a key: :space), or as {keys: [...], mouse: [...], buttons: [...]}, mixed as one likes.
module Input
  FIRST_PAD = 0
  KEY_PREFIX = "key:".freeze
  MOUSE_PREFIX = "mouse:".freeze
  PAD_PREFIX = "pad:".freeze
  # What calls a capture off unless it is told otherwise: Escape, and a gamepad's B or Back.
  CANCEL_CAPTURE = ["key:escape", "pad:b", "pad:back"].freeze
  RUMBLE_SUPPORTED = false     # (see Input.rumble?)
  RUMBLE_STRENGTHS = (0..1)    # from still to as hard as a gamepad shakes

  def self.axis(name, pad: FIRST_PAD) = __axis(name, __pad(pad))
  def self.action?(name, pad: FIRST_PAD) = __action_down(name, __pad(pad))
  def self.action_pressed?(name, pad: FIRST_PAD) = __action_pressed(name, __pad(pad))
  def self.action_released?(name, pad: FIRST_PAD) = __action_released(name, __pad(pad))

  def self.gamepad?(pad: FIRST_PAD) = __gamepad(__pad(pad))
  def self.gamepad_axis(axis, pad: FIRST_PAD) = __gamepad_axis(axis, __pad(pad))
  def self.gamepad_down?(button, pad: FIRST_PAD) = __gamepad_down(button, __pad(pad))
  def self.gamepad_pressed?(button, pad: FIRST_PAD) = __gamepad_pressed(button, __pad(pad))
  def self.gamepad_released?(button, pad: FIRST_PAD) = __gamepad_released(button, __pad(pad))

  # Whether gamepads can rumble here: never yet. The window library the engine runs on (raylib 5.5)
  # has no vibration on desktops nor on Android, so Input.rumble is a hook that does nothing until
  # one does; a game asks this before offering a "vibration" option.
  def self.rumble? = RUMBLE_SUPPORTED

  # Rumbles a player's gamepad at `strength` (0 to 1) for `seconds`. Does nothing for now (see
  # rumble?) and says so: false. A gamepad's number (checked as everywhere else), a strength or a
  # time out of range is an error all the same.
  def self.rumble(pad, strength, seconds)
    raise ArgumentError, "rumble strength goes from 0 to 1" unless RUMBLE_STRENGTHS.cover?(strength)
    raise ArgumentError, "rumble time is a number of seconds, not negative" unless __rumble_time?(seconds)

    gamepad?(pad: pad) && rumble?
  end

  def self.__rumble_time?(seconds) = seconds.is_a?(Numeric) && seconds >= 0

  # Input.define_action(:dash, keys: [:left_shift], buttons: [:b]), define_action(:fire, mouse:
  # [:left]), or define_action(:dash, :e, "pad:b"). Declaring a name again binds it anew.
  def self.define_action(name, *bindings, keys: [], buttons: [], mouse: [])
    __changed
    __define_action(name, __texts(bindings) + __texts(keys: keys, mouse: mouse, buttons: buttons))
  end

  # Input.define_axis(:steer, negative: [:a], positive: {keys: [:d], buttons: [:dpad_right]},
  # pad_axis: :left_x). invert: true runs the stick the other way (a stick pushed up reads negative).
  def self.define_axis(name, negative: [], positive: [], pad_axis: nil, invert: false)
    __changed
    __define_axis(name, __texts(negative), __texts(positive), pad_axis, invert)
  end

  # Adds keys or buttons to what a player's action triggers on: Input.bind(:jump, key: :k),
  # Input.bind(:jump, button: :y, pad: 1), Input.bind(:fire, mouse: :left), Input.bind(:jump,
  # "key:k"); given several of key:, mouse: and button:, it binds them all. An axis says which way
  # the binding pushes it: Input.bind(:horizontal, key: :j, side: :negative). Kept for the next run.
  def self.bind(name, binding = nil, key: nil, button: nil, mouse: nil, side: nil, pad: FIRST_PAD)
    texts = __given(binding, key, button, mouse)
    raise ArgumentError, "say what to bind: key:, mouse:, button: or a binding" if texts.empty?

    __changed
    texts.each { |text| __bind(name, text, side, __pad(pad)) }
    true
  end

  # Takes the bindings said away (all those given), or all of an action's keys and buttons when
  # none is said.
  def self.unbind(name, binding = nil, key: nil, button: nil, mouse: nil, side: nil, pad: FIRST_PAD)
    texts = __given(binding, key, button, mouse)
    __changed
    return __unbind(name, nil, side, __pad(pad)) if texts.empty?

    texts.each { |text| __unbind(name, text, side, __pad(pad)) }
    true
  end

  # An action's bindings (["key:space", "pad:a"]), or an axis' ({negative: [...], positive: [...]}).
  # (The stick that moves an axis is the game's, the same for every player: Input.axis_stick.)
  def self.bindings(name, pad: FIRST_PAD) = __bindings(name, __pad(pad))

  # Back to the game's own bindings: one name or all, for one player or (pad: nil) all of them.
  def self.reset_bindings(name = nil, pad: nil)
    __changed
    __reset_bindings(name, pad)
  end

  # How many times the actions, the axes or what they are bound to may have changed: a screen that
  # shows them works them out again when it is not the number it saw last.
  def self.bindings_changes = @bindings_changes.to_i

  def self.__changed = @bindings_changes = bindings_changes + 1

  # Calls the block with the next key, mouse button or gamepad button pressed, or the mouse wheel
  # turned ("key:k", "mouse:left", "mouse:wheel_up", or "pad:a" with the gamepad's number): what a
  # "press a key" screen waits for. A click already held when it is asked for (the one that asked)
  # does not count, and the click caught is not seen by anything else, its release neither.
  # mouse: false leaves the mouse out (its clicks still work the screen's buttons). One of `cancel`
  # calls the capture off instead: the block gets nil for the binding. Asking again replaces the block.
  def self.capture_next(cancel: CANCEL_CAPTURE, mouse: true, &block)
    @capture = block
    __capture(__texts(cancel), mouse)
  end

  def self.cancel_capture
    @capture = nil
    __cancel_capture
  end

  # Whether a capture still waits to call this block (not once it was called, called off, or
  # another capture took its place).
  def self.capturing_for?(block) = capturing? && @capture.equal?(block)

  # A capture asked for by a scene that is gone is called off with it.
  def self.__scene_reset = cancel_capture

  # The engine's entry point when a capture caught something (nil: it was called off); pad is nil
  # for a key or the mouse.
  def self.__captured(binding, pad)
    block = @capture
    @capture = nil
    block&.call(binding, pad)
  end

  def self.__pad(pad) = pad || FIRST_PAD

  def self.__given(binding, key, button, mouse)
    __texts(binding.nil? ? [] : [binding]) + __texts(keys: Array(key), mouse: Array(mouse), buttons: Array(button))
  end

  # Bindings in any of the ways they may be written, as text.
  def self.__texts(bindings)
    return __texts_of_hash(bindings) if bindings.is_a?(Hash)

    Array(bindings).map { |one| one.is_a?(Symbol) ? "#{KEY_PREFIX}#{one}" : one.to_s }
  end

  def self.__texts_of_hash(bindings)
    keys = bindings.fetch(:keys, []).map { |key| "#{KEY_PREFIX}#{key}" }
    mouse = bindings.fetch(:mouse, []).map { |input| "#{MOUSE_PREFIX}#{input}" }
    keys + mouse + bindings.fetch(:buttons, []).map { |button| "#{PAD_PREFIX}#{button}" }
  end
end
