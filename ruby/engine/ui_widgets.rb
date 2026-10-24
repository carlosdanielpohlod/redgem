# What settings screens are made of: toggles, choices and sliders, drawn as rows with a label on
# the left and their value on the right, and a focus that the keyboard and a gamepad move between
# them. Like the rest of UI they are immediate: draw them every frame, keep what they return and
# pass it back.
#
#   @focus ||= UI::Focus.new
#   @focus.update(3)                                            # up and down move it
#   @vsync = UI.toggle("VSync", @vsync, x: 40, y: 100, width: 480, height: 44, focused: @focus.at?(0))
#   @mode = UI.choice("Mode", %w[Windowed Fullscreen], @mode, x: 40, y: 152, width: 480, height: 44,
#                     focused: @focus.at?(1))
#   @volume = UI.slider("Volume", @volume, x: 40, y: 204, width: 480, height: 44, focused: @focus.at?(2))
#
# A focused widget is ringed. The confirm key (Enter) or a gamepad's A works it as a click does;
# left and right (arrows, D-pad) change a toggle, a choice or a slider.
module UI
  # [keys, gamepad buttons] that do each thing.
  UP = [%i[up], %i[dpad_up]].freeze
  DOWN = [%i[down], %i[dpad_down]].freeze
  LEFT = [%i[left], %i[dpad_left]].freeze
  RIGHT = [%i[right], %i[dpad_right]].freeze
  CONFIRM = [%i[enter], %i[a]].freeze
  FOCUS_COLOR = :gold
  FOCUS_RING = 3            # pixels the ring around a focused widget shows past it
  SLIDER_STEP = 0.05        # a slider's step unless said
  SLIDER_SHARE = 0.45       # how much of its row a slider's track takes, on the right
  SLIDER_TRACK_COLOR = :gray
  SLIDER_FILL_COLOR = :ruby
  SLIDER_THICKNESS = 0.18   # the track's height, as a share of the row's

  # Which of a screen's widgets the keyboard and a gamepad work: up and down move it, wrapping.
  class Focus
    attr_reader :index

    def initialize
      @index = 0
    end

    def reset
      @index = 0
    end

    # Moves among `count` widgets, as up or down was pressed this frame.
    def update(count)
      return @index = 0 unless count.positive?

      @index = (@index + UI.direction(UP, DOWN)) % count
    end

    def at?(index) = @index == index
  end

  # Whether one of the keys or gamepad buttons of `controls` ([keys, buttons]) was pressed this frame.
  def self.pressed_any?(controls)
    keys, buttons = controls
    keys.any? { |key| Input.pressed?(key) } || buttons.any? { |button| Input.gamepad_pressed?(button) }
  end

  # 1 when `forth` was pressed this frame, -1 for `back`, 0 for neither (or both).
  def self.direction(back, forth) = (pressed_any?(forth) ? 1 : 0) - (pressed_any?(back) ? 1 : 0)

  # -1 (left), 1 (right) or 0, as pressed this frame.
  def self.sideways = direction(LEFT, RIGHT)

  def self.confirmed? = pressed_any?(CONFIRM)

  # Whether a widget is worked this frame as a click does: clicked, or confirmed while focused.
  def self.worked?(box, focused) = clicked?(mouse_over?(**box)) || (focused && confirmed?)

  # Whether a toggle flips this frame: worked, or pushed left or right while focused.
  def self.flipped?(box, focused) = worked?(box, focused) || (focused && sideways != 0)

  # A ring around a focused widget's box.
  def self.focus_ring(box)
    rect(x: box[:x] - FOCUS_RING, y: box[:y] - FOCUS_RING, width: box[:width] + 2 * FOCUS_RING,
         height: box[:height] + 2 * FOCUS_RING, color: FOCUS_COLOR, radius: BUTTON_RADIUS + FOCUS_RING)
  end

  # A widget's row: its label on the left, its value (when it has one) on the right.
  def self.row(label, value, box, focused)
    focus_ring(box) if focused
    rect(**box, color: BUTTON_COLOR, alpha: BUTTON_ALPHA, radius: BUTTON_RADIUS)
    size = label_size(box[:height])
    y = centered(box[:y], box[:height], size)
    text(label, x: box[:x] + FIELD_PADDING, y: y, size: size)
    text(value, x: box[:x] + box[:width] - FIELD_PADDING, y: y, size: size, align: :right) if value
  end

  # A switch: returns whether it is on after this frame. A click flips it, and so do the confirm
  # key or button and left or right while it is focused.
  def self.toggle(label, state, x:, y:, width:, height:, focused: false)
    box = { x: x, y: y, width: width, height: height }
    state = !state if flipped?(box, focused)
    row(label, state ? I18n.t("game.settings.on") : I18n.t("game.settings.off"), box, focused)
    state
  end

  # One of `choices` (texts): returns the index of the one chosen after this frame. Left and right
  # go through them while it is focused; a click (or the confirm key or button) goes to the next.
  def self.choice(label, choices, index, x:, y:, width:, height:, focused: false)
    box = { x: x, y: y, width: width, height: height }
    step = worked?(box, focused) ? 1 : 0
    step = sideways if focused && step.zero?
    index = (index + step) % choices.size
    row(label, "< #{choices[index]} >", box, focused)
    index
  end

  # A number from `min` to `max`: returns it after this frame. The mouse held on its track sets
  # it; left and right move it a step while it is focused.
  def self.slider(label, value, x:, y:, width:, height:, min: 0.0, max: 1.0, step: SLIDER_STEP, focused: false)
    box = { x: x, y: y, width: width, height: height }
    track = slider_track(box)
    value += sideways * step if focused
    value = slider_at(track, min, max) if held?(mouse_over?(**track))
    value = value.clamp(min, max)
    row(label, nil, box, focused)
    draw_slider(track, (value - min) / (max - min).to_f)
    value
  end

  # Where a row's slider track is: on its right, as tall as its middle.
  def self.slider_track(box)
    width = (box[:width] * SLIDER_SHARE).to_i
    { x: box[:x] + box[:width] - FIELD_PADDING - width, y: box[:y], width: width, height: box[:height] }
  end

  # The value the pointer points at along a track.
  def self.slider_at(track, min, max)
    share = (Input.mouse_x - track[:x]).to_f / track[:width]
    min + share.clamp(0.0, 1.0) * (max - min)
  end

  # A track filled up to `share` (0 to 1).
  def self.draw_slider(track, share)
    thickness = (track[:height] * SLIDER_THICKNESS).to_i
    bar = { x: track[:x], y: centered(track[:y], track[:height], thickness), height: thickness }
    rect(**bar, width: track[:width], color: SLIDER_TRACK_COLOR, radius: thickness / 2)
    rect(**bar, width: (track[:width] * share).to_i, color: SLIDER_FILL_COLOR, radius: thickness / 2)
  end
end
