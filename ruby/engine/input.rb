# A game's actions and axes, for each player. A player is a gamepad's number: 0 is the first (who
# also has the keyboard), up to 3; pad: nil is the first too. Bindings are written as text
# ("key:space" is a key, "pad:a" a gamepad button), as a Symbol (a key: :space), or as
# {keys: [...], buttons: [...]}, mixed as one likes.
module Input
  FIRST_PAD = 0
  KEY_PREFIX = "key:".freeze
  PAD_PREFIX = "pad:".freeze
  # What calls a capture off unless it is told otherwise: Escape, and a gamepad's B or Back.
  CANCEL_CAPTURE = ["key:escape", "pad:b", "pad:back"].freeze

  def self.axis(name, pad: FIRST_PAD) = __axis(name, __pad(pad))
  def self.action?(name, pad: FIRST_PAD) = __action_down(name, __pad(pad))
  def self.action_pressed?(name, pad: FIRST_PAD) = __action_pressed(name, __pad(pad))
  def self.action_released?(name, pad: FIRST_PAD) = __action_released(name, __pad(pad))

  def self.gamepad?(pad: FIRST_PAD) = __gamepad(__pad(pad))
  def self.gamepad_axis(axis, pad: FIRST_PAD) = __gamepad_axis(axis, __pad(pad))
  def self.gamepad_down?(button, pad: FIRST_PAD) = __gamepad_down(button, __pad(pad))
  def self.gamepad_pressed?(button, pad: FIRST_PAD) = __gamepad_pressed(button, __pad(pad))
  def self.gamepad_released?(button, pad: FIRST_PAD) = __gamepad_released(button, __pad(pad))

  # Input.define_action(:dash, keys: [:left_shift], buttons: [:b]), or define_action(:dash, :e,
  # "pad:b"). Declaring a name again binds it anew.
  def self.define_action(name, *bindings, keys: [], buttons: [])
    __define_action(name, __texts(bindings) + __texts(keys: keys, buttons: buttons))
  end

  # Input.define_axis(:steer, negative: [:a], positive: {keys: [:d], buttons: [:dpad_right]},
  # pad_axis: :left_x). invert: true runs the stick the other way (a stick pushed up reads negative).
  def self.define_axis(name, negative: [], positive: [], pad_axis: nil, invert: false)
    __define_axis(name, __texts(negative), __texts(positive), pad_axis, invert)
  end

  # Adds keys or buttons to what a player's action triggers on: Input.bind(:jump, key: :k),
  # Input.bind(:jump, button: :y, pad: 1), Input.bind(:jump, "key:k"); given both key: and
  # button:, it binds both. An axis says which way the binding pushes it: Input.bind(:horizontal,
  # key: :j, side: :negative). Kept for the next run.
  def self.bind(name, binding = nil, key: nil, button: nil, side: nil, pad: FIRST_PAD)
    texts = __given(binding, key, button)
    raise ArgumentError, "say what to bind: key:, button: or a binding" if texts.empty?

    texts.each { |text| __bind(name, text, side, __pad(pad)) }
    true
  end

  # Takes the bindings said away (all those given), or all of an action's keys and buttons when
  # none is said.
  def self.unbind(name, binding = nil, key: nil, button: nil, side: nil, pad: FIRST_PAD)
    texts = __given(binding, key, button)
    return __unbind(name, nil, side, __pad(pad)) if texts.empty?

    texts.each { |text| __unbind(name, text, side, __pad(pad)) }
    true
  end

  # An action's bindings (["key:space", "pad:a"]), or an axis' ({negative: [...], positive: [...]}).
  def self.bindings(name, pad: FIRST_PAD) = __bindings(name, __pad(pad))

  # Back to the game's own bindings: one name or all, for one player or (pad: nil) all of them.
  def self.reset_bindings(name = nil, pad: nil) = __reset_bindings(name, pad)

  # Calls the block with the next key or gamepad button pressed ("key:k", or "pad:a" with the
  # gamepad's number): what a "press a key" screen waits for. One of `cancel` calls the capture
  # off instead: the block gets nil for the binding. Asking again replaces the block.
  def self.capture_next(cancel: CANCEL_CAPTURE, &block)
    @capture = block
    __capture(__texts(cancel))
  end

  def self.cancel_capture
    @capture = nil
    __cancel_capture
  end

  # A capture asked for by a scene that is gone is called off with it.
  def self.__scene_reset = cancel_capture

  # The engine's entry point when a capture caught something (nil: it was called off); pad is nil
  # for a key.
  def self.__captured(binding, pad)
    block = @capture
    @capture = nil
    block&.call(binding, pad)
  end

  def self.__pad(pad) = pad || FIRST_PAD

  def self.__given(binding, key, button)
    __texts(binding.nil? ? [] : [binding]) + __texts(keys: Array(key), buttons: Array(button))
  end

  # Bindings in any of the ways they may be written, as text.
  def self.__texts(bindings)
    return __texts_of_hash(bindings) if bindings.is_a?(Hash)

    Array(bindings).map { |one| one.is_a?(Symbol) ? "#{KEY_PREFIX}#{one}" : one.to_s }
  end

  def self.__texts_of_hash(bindings)
    keys = bindings.fetch(:keys, []).map { |key| "#{KEY_PREFIX}#{key}" }
    keys + bindings.fetch(:buttons, []).map { |button| "#{PAD_PREFIX}#{button}" }
  end
end
