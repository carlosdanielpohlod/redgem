# Drawing over the game: HUDs and menus. Everything here is "immediate": call it every frame from
# a behaviour's update, and it is drawn that frame. Positions are in pixels from the top-left
# corner; UI.width and UI.height are the size of what the game is drawn in.
#
#   UI.text("Score: #{@score}", x: 20, y: 20, size: 28, color: :gold)
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
  def self.text(text, x:, y:, size: DEFAULT_SIZE, color: :white, alpha: OPAQUE, align: :left)
    text = text.to_s
    x -= text_width(text, size: size) / 2 if align == :center
    x -= text_width(text, size: size) if align == :right
    r, g, b = rgb(color)
    __text(text, x.to_i, y.to_i, size.to_i, r, g, b, alpha.to_i)
  end

  def self.text_width(text, size: DEFAULT_SIZE) = __text_width(text.to_s, size.to_i)

  # A filled rectangle; radius rounds its corners (pixels).
  def self.rect(x:, y:, width:, height:, color: :black, alpha: OPAQUE, radius: 0)
    r, g, b = rgb(color)
    __rect(x.to_i, y.to_i, width.to_i, height.to_i, r, g, b, alpha.to_i, radius.to_f)
  end

  # A picture from the game's images/ folder (UI.image(:logo) draws images/logo.png). Without a
  # width and height it is drawn at its own size; with only one, the other follows its shape.
  def self.image(name, x:, y:, width: 0, height: 0, alpha: OPAQUE)
    __image(name.to_s, x.to_i, y.to_i, width.to_i, height.to_i, alpha.to_i)
  end

  # A box to type in. Returns the text after this frame's typing: keep it and pass it back. A
  # secret one (a password) shows a star for each letter.
  #   @name = UI.text_field(@name, x: 40, y: 200, width: 320, height: 48)
  def self.text_field(value, x:, y:, width:, height:, max_length: FIELD_MAX_LENGTH, color: BUTTON_COLOR,
                      text_color: :white, secret: false)
    value = value.to_s + Input.text
    value = chop(value) if Input.pressed?(:backspace)
    value = chop(value) while letters(value) > max_length
    rect(x: x, y: y, width: width, height: height, color: color, alpha: BUTTON_ALPHA, radius: BUTTON_RADIUS)
    size = label_size(height)
    shown = secret ? SECRET_LETTER * letters(value) : value
    text(shown + FIELD_CARET, x: x + FIELD_PADDING, y: centered(y, height, size), size: size, color: text_color)
    value
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

  def self.mouse_over?(x:, y:, width:, height:)
    Input.mouse_x >= x && Input.mouse_x < x + width && Input.mouse_y >= y && Input.mouse_y < y + height
  end

  # Draws a button and returns true on the frame it is clicked (the left button is released over
  # it).
  def self.button(label, x:, y:, width:, height:, color: BUTTON_COLOR, hover_color: BUTTON_HOVER_COLOR,
                  text_color: :white, size: nil)
    over = mouse_over?(x: x, y: y, width: width, height: height)
    fill = button_fill(over ? hover_color : color, pressed: held?(over))
    rect(x: x, y: y, width: width, height: height, color: fill, alpha: BUTTON_ALPHA, radius: BUTTON_RADIUS)
    size ||= label_size(height)
    text(label, x: centered(x, width, 0), y: centered(y, height, size), size: size, color: text_color, align: :center)
    clicked?(over)
  end

  # What the left button of the mouse does to a button the pointer may be `over`.
  def self.held?(over) = over && Input.mouse_down?(:left)
  def self.clicked?(over) = over && Input.mouse_released?(:left)

  # A held button is darker.
  def self.button_fill(color, pressed:) = pressed ? shade(color, PRESSED_SHADE) : rgb(color)
end
