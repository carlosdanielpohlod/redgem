# Drawing over the game: HUDs and menus. Everything here is "immediate": call it every frame from
# a behaviour's update, and it is drawn that frame. Positions are in pixels from the top-left
# corner; UI.width and UI.height are the size of what the game is drawn in. With an anchor (see
# ui_layout.rb), x and y are offsets from a corner, an edge's middle or the middle of the screen.
#
#   UI.text("Score: #{@score}", x: 20, y: 20, size: 28, color: :gold)
#   UI.text("Lives: #{@lives}", anchor: :top_right, x: -20, y: 20)
#   Game.quit if UI.button("Quit", x: 40, y: 200, width: 220, height: 56)
module UI
  DEFAULT_SIZE = 24
  OPAQUE = 255

  COLORS = {
    white: [255, 255, 255],
    black: [0, 0, 0],
    red: [230, 41, 55],
    green: [0, 228, 48],
    blue: [0, 121, 241],
    yellow: [253, 249, 0],
    gold: [255, 203, 0],
    orange: [255, 161, 0],
    gray: [130, 130, 130],
    dark: [24, 20, 24],
    ruby: [225, 29, 72]
  }.freeze

  # Looks of a button: resting, under the mouse, pressed.
  BUTTON_COLOR = :dark
  BUTTON_HOVER_COLOR = :ruby
  BUTTON_ALPHA = 225
  BUTTON_RADIUS = 10
  BUTTON_TEXT_SHARE = 0.42   # text height as a share of the button's height
  FIELD_MAX_LENGTH = 64
  FIELD_PADDING = 14         # pixels between a text field's edge and its text
  FIELD_CARET = "_".freeze
  SECRET_LETTER = "*".freeze  # what a secret text field shows for each letter
  CONTINUATION_MASK = 0xC0
  CONTINUATION_BITS = 0x80
  PRESSED_SHADE = 0.8        # a held button is this much darker

  # A color [r, g, b] darkened to a share of its brightness.
  def self.shade(color, share) = rgb(color).map { |channel| (channel * share).to_i }

  # How tall the text of a button or a text field of that height is.
  def self.label_size(height) = (height * BUTTON_TEXT_SHARE).to_i

  # Where something `size` tall starts to sit in the middle of a span from `start`, `length` long.
  def self.centered(start, length, size) = start + (length - size) / 2

  def self.rgb(color)
    color.is_a?(Array) ? color : COLORS.fetch(color) { raise ArgumentError, "unknown color #{color.inspect}" }
  end

  # align: :left (x is the left edge), :center (x is the middle) or :right (x is the right edge).
  # width: the text wraps at word boundaries to lines no wider (UI.wrap); each line, and each of
  # the text's own line breaks, starts LINE_SPACING times the size below the last. With an anchor,
  # the text's box (as wide as width:, or its widest line) sits at it by its matching corner, and
  # align lays the lines out inside that box.
  def self.text(text, x:, y:, size: DEFAULT_SIZE, color: :white, alpha: OPAQUE, align: :left, width: nil,
                anchor: nil)
    lines = text_lines(text, width, size)
    box = { width: text_box_width(lines, size, width, anchor, align), height: lines_height(lines.size, size) }
    left, top = text_corner(box, x, y, anchor, align)
    block = TextBlock.new(left, top, box[:width], size, align)
    look = [size.to_i, *rgb(color), alpha.to_i]
    lines.each_with_index { |line, row| __text(line, *block.line_at(line, row), *look) }
  end

  def self.text_width(text, size: DEFAULT_SIZE) = __text_width(text.to_s, size.to_i)

  # A filled rectangle; radius rounds its corners (pixels).
  def self.rect(x:, y:, width:, height:, color: :black, alpha: OPAQUE, radius: 0, anchor: nil)
    box = placed(anchor, x, y, width, height)
    __rect(*box.values_at(:x, :y, :width, :height).map(&:to_i), *rgb(color), alpha.to_i, radius.to_f)
  end

  # A picture from the game's images/ folder (UI.image(:logo) draws images/logo.png). Without a
  # width and height it is drawn at its own size; with only one, the other follows its shape. An
  # anchor places it by the width and height given (0 for one not given).
  def self.image(name, x:, y:, width: 0, height: 0, alpha: OPAQUE, anchor: nil)
    box = placed(anchor, x, y, width, height)
    __image(name.to_s, *box.values_at(:x, :y, :width, :height).map(&:to_i), alpha.to_i)
  end

  # A box to type in. Returns the text after this frame's typing: keep it and pass it back. A
  # secret one (a password) shows a star for each letter.
  #   @name = UI.text_field(@name, x: 40, y: 200, width: 320, height: 48)
  def self.text_field(value, x:, y:, width:, height:, max_length: FIELD_MAX_LENGTH, color: BUTTON_COLOR,
                      text_color: :white, secret: false, anchor: nil)
    value = value.to_s + Input.text
    value = chop(value) if Input.pressed?(:backspace)
    value = chop(value) while letters(value) > max_length
    shown = secret ? SECRET_LETTER * letters(value) : value
    draw_field(shown, placed(anchor, x, y, width, height), color, text_color)
    value
  end

  # A text field's box and what it shows, with the caret after it.
  def self.draw_field(shown, box, color, text_color)
    rect(**box, color: color, alpha: BUTTON_ALPHA, radius: BUTTON_RADIUS)
    size = label_size(box[:height])
    text(shown + FIELD_CARET, x: box[:x] + FIELD_PADDING, y: centered(box[:y], box[:height], size), size: size,
                              color: text_color)
  end

  # A text without its last letter: all of that letter's bytes (accented letters take several).
  def self.chop(text)
    size = text.bytesize - 1
    size -= 1 while inside_letter?(text, size)
    size.positive? ? text.byteslice(0, size) : ""
  end

  # Whether the byte at `index` is one of a letter's later bytes: the letter starts before it.
  def self.inside_letter?(text, index) = index.positive? && continuation?(text.getbyte(index))

  # How many letters a text has, however many bytes each takes.
  def self.letters(text) = text.bytes.count { |byte| !continuation?(byte) }

  # In UTF-8, the bytes of a letter after its first all look like 10xxxxxx.
  def self.continuation?(byte) = (byte & CONTINUATION_MASK) == CONTINUATION_BITS

  # Whether the pointer is over a box, and inside what UI.clip draws in (if it draws in one).
  def self.mouse_over?(x:, y:, width:, height:)
    pointer_in?({ x: x, y: y, width: width, height: height }) && pointer_in_clip?
  end

  # Draws a button and returns true on the frame it is clicked (the left button is released over
  # it). A focused one (see UI::Focus) is ringed, and the confirm key or a gamepad's A clicks it
  # (`confirm: UI::GAMEPAD_CONFIRM` leaves Enter to a text field).
  def self.button(label, x:, y:, width:, height:, color: BUTTON_COLOR, hover_color: BUTTON_HOVER_COLOR,
                  text_color: :white, size: nil, focused: false, confirm: CONFIRM, anchor: nil)
    box = placed(anchor, x, y, width, height)
    over = mouse_over?(**box)
    focus_ring(box) if focused
    fill = button_fill(over || focused ? hover_color : color, pressed: held?(over))
    rect(**box, color: fill, alpha: BUTTON_ALPHA, radius: BUTTON_RADIUS)
    size ||= label_size(height)
    text(label, x: centered(box[:x], width, 0), y: centered(box[:y], height, size), size: size, color: text_color,
                align: :center)
    worked?(box, focused, confirm)
  end

  # What the left button of the mouse does to a button the pointer may be `over`.
  def self.held?(over) = over && Input.mouse_down?(:left)
  def self.clicked?(over) = over && Input.mouse_released?(:left)

  # A held button is darker.
  def self.button_fill(color, pressed:) = pressed ? shade(color, PRESSED_SHADE) : rgb(color)
end
