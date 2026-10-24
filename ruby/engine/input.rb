# A game's actions and axes, for each player. A player is a gamepad's number: 0 is the first (who
# also has the keyboard), up to 3. Bindings are text: "key:space" is a key, "pad:a" a gamepad button.
module Input
  FIRST_PAD = 0
  KEY_PREFIX = "key:".freeze
  PAD_PREFIX = "pad:".freeze

  def self.axis(name, pad: FIRST_PAD) = __axis(name, pad)
  def self.action?(name, pad: FIRST_PAD) = __action_down(name, pad)
  def self.action_pressed?(name, pad: FIRST_PAD) = __action_pressed(name, pad)
  def self.action_released?(name, pad: FIRST_PAD) = __action_released(name, pad)

  def self.gamepad?(pad: FIRST_PAD) = __gamepad(pad)
  def self.gamepad_axis(axis, pad: FIRST_PAD) = __gamepad_axis(axis, pad)
  def self.gamepad_down?(button, pad: FIRST_PAD) = __gamepad_down(button, pad)
  def self.gamepad_pressed?(button, pad: FIRST_PAD) = __gamepad_pressed(button, pad)
  def self.gamepad_released?(button, pad: FIRST_PAD) = __gamepad_released(button, pad)

  # Input.define_action(:dash, keys: [:left_shift], buttons: [:b]). Declaring a name again binds it anew.
  def self.define_action(name, keys: [], buttons: [])
    __define_action(name, keys.map { |key| "#{KEY_PREFIX}#{key}" } + buttons.map { |button| "#{PAD_PREFIX}#{button}" })
  end

  # Input.define_axis(:steer, negative: ["key:a"], positive: ["key:d"], pad_axis: :left_x).
  # invert: true runs the stick the other way (a stick pushed up reads negative).
  def self.define_axis(name, negative: [], positive: [], pad_axis: nil, invert: false)
    __define_axis(name, negative.map(&:to_s), positive.map(&:to_s), pad_axis, invert)
  end

  # Adds a key or a button to what a player's action triggers on: Input.bind(:jump, key: :k),
  # Input.bind(:jump, button: :y, pad: 1), Input.bind(:jump, "key:k"). An axis says which way the
  # binding pushes it: Input.bind(:horizontal, key: :j, side: :negative). Kept for the next run.
  def self.bind(name, binding = nil, key: nil, button: nil, side: nil, pad: FIRST_PAD)
    __bind(name, __binding_text(binding, key, button), side, pad)
  end

  # Takes one binding away, or all of an action's keys and buttons when none is said.
  def self.unbind(name, binding = nil, key: nil, button: nil, side: nil, pad: FIRST_PAD)
    __unbind(name, __binding_text(binding, key, button), side, pad)
  end

  # An action's bindings (["key:space", "pad:a"]), or an axis' ({negative: [...], positive: [...]}).
  def self.bindings(name, pad: FIRST_PAD) = __bindings(name, pad)

  # Back to the game's own bindings: one name or all, for one player or (pad: nil) all of them.
  def self.reset_bindings(name = nil, pad: nil) = __reset_bindings(name, pad)

  # Calls the block with the next key or gamepad button pressed ("key:k", or "pad:a" with the
  # gamepad's number): what a "press a key" screen waits for. Asking again replaces the block.
  def self.capture_next(&block)
    @capture = block
    __capture
  end

  def self.cancel_capture
    @capture = nil
    __cancel_capture
  end

  # The engine's entry point when a capture caught something; pad is nil for a key.
  def self.__captured(binding, pad)
    block = @capture
    @capture = nil
    block&.call(binding, pad)
  end

  def self.__binding_text(binding, key, button)
    return binding.to_s if binding
    return "#{KEY_PREFIX}#{key}" if key

    "#{PAD_PREFIX}#{button}" if button
  end
end
