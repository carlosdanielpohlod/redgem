# How a button is drawn, as the theme says (see UI::Theme): its fill for each state, its border or
# its picture (a skin cut in nine, whose corners keep their size), an icon before its label, and
# the ring around it while it has the focus. UI.button draws one and hears the mouse and the keys;
# a game that draws its own buttons (GameMenu's draw_button) gets the same state.
module UI
  # What a button is this frame: focused (UI::Focus), under the pointer (hover), held down by it
  # (pressed), disabled; and the option it is of (a menu's, by id: nil for any other).
  class ButtonState
    attr_reader :id

    def initialize(focused: false, hover: false, pressed: false, disabled: false, id: nil)
      @flags = { focused: focused, hover: hover, pressed: pressed, disabled: disabled }
      @id = id
    end

    def focused? = @flags[:focused]
    def hover? = @flags[:hover]
    def pressed? = @flags[:pressed]
    def disabled? = @flags[:disabled]

    # The one that shows most: :disabled, :pressed, :hover, :focused or :normal.
    def name = %i[disabled pressed hover focused].find { |flag| @flags[flag] } || :normal
  end

  # Draws a button in a box ({x:, y:, width:, height:}) as it is in `state` (a UI::ButtonState),
  # without hearing anything. look: color, hover_color, text_color, size and icon, each the
  # theme's unless given.
  def self.draw_button(label, box, state, look = {})
    focus_ring(box) if state.focused?
    face(box, button_fill(state, look), button_skin(state, look))
    size = look[:size] || label_size(box[:height])
    x = centered(box[:x], box[:width], 0)
    x = button_icon(look[:icon], box, label, size) if look[:icon]
    text(label, x: x, y: centered(box[:y], box[:height], size), size: size, color: label_color(state, look),
                align: :center)
  end

  # A button's fill in a state, [r, g, b]: held down, it is darker.
  def self.button_fill(state, look = {})
    return rgb(theme[:colors, :disabled]) if state.disabled?

    fill = rgb(state_color(state, look))
    state.pressed? ? pressed_fill(fill) : fill
  end

  # (A color the call gives for being under the pointer is also the focused one's, as it always was.)
  def self.state_color(state, look)
    given = state.hover? || state.focused? ? look[:hover_color] : look[:color]
    given || theme[:colors, state_color_name(state)]
  end

  def self.state_color_name(state)
    return :hover if state.hover?

    state.focused? ? :focused : :button
  end

  def self.pressed_fill(fill)
    pressed = theme[:colors, :pressed]
    pressed ? rgb(pressed) : shade(fill, theme[:colors, :pressed_shade])
  end

  def self.label_color(state, look)
    state.disabled? ? theme[:colors, :disabled_text] : look[:text_color] || theme[:colors, :label]
  end

  # The skin of a state: its own, else the button's (nil: drawn as a rounded box).
  # Whether a button was given colors of its own, which a theme's skin would hide.
  def self.own_fill?(look) = look[:color] || look[:hover_color]

  # The theme's skin for a state (nil when the button has colors of its own).
  def self.button_skin(state, look = {})
    return if own_fill?(look)

    own = theme[:button, :"skin_#{state.name}"] if %i[focused pressed disabled].include?(state.name)
    own || theme[:button, :skin]
  end

  # What a button, a row or a text field sits on: a skin, or a rounded box with a border when the
  # theme gives it one.
  def self.face(box, fill, skin = nil)
    return nine_slice(skin, box) if skin

    border = theme[:button, :border]
    radius = theme[:button, :radius]
    alpha = theme[:colors, :button_alpha]
    rect(**box, color: theme[:colors, :border], alpha: alpha, radius: radius) if border.positive?
    rect(**inset(box, border), color: fill, alpha: alpha, radius: [radius - border, 0].max)
  end

  # A box `units` smaller on every side.
  def self.inset(box, units)
    return box if units.zero?

    { x: box[:x] + units, y: box[:y] + units, width: box[:width] - 2 * units, height: box[:height] - 2 * units }
  end

  # A ring around a focused widget's box.
  def self.focus_ring(box)
    ring = theme[:button, :focus_ring]
    return if ring.zero?

    rect(x: box[:x] - ring, y: box[:y] - ring, width: box[:width] + 2 * ring, height: box[:height] + 2 * ring,
         color: theme[:colors, :focus_ring], radius: theme[:button, :radius] + ring)
  end

  # A picture over a box, cut in nine: its corners as large as the theme's skin_edge (in its own
  # pixels and in units alike), its edges stretched along, its middle across.
  def self.nine_slice(name, box)
    size = image_size(name)
    return image(name, **box, alpha: theme[:colors, :button_alpha]) unless size

    columns = shown_slices(size[0], box[:x], box[:width])
    shown_slices(size[1], box[:y], box[:height]).each { |row| columns.each { |column| slice(name, column, row) } }
  end

  def self.shown_slices(picture, start, length) = slices(picture, start, length).select { |part| slice_shows?(part) }

  # Whether a slice has some of the picture and room to draw it: a picture no larger than its
  # corners has no middle, and neither has a box no larger than them. (A part or a size of 0 would
  # be read as the whole picture.)
  def self.slice_shows?(slice) = slice[1].positive? && slice[3].positive?

  # The three slices along one side: [from, length in the picture, at, length drawn] each. A box
  # too small for the corners draws them smaller, whole.
  def self.slices(picture, start, length)
    edge = [theme[:button, :skin_edge], picture / 2].min
    drawn = [edge, length / 2].min
    middle = length - 2 * drawn
    [[0, edge, start, drawn], [edge, picture - 2 * edge, start + drawn, middle],
     [picture - edge, edge, start + drawn + middle, drawn]]
  end

  def self.slice(name, column, row)
    image(name, x: column[2], y: row[2], width: column[3], height: row[3], alpha: theme[:colors, :button_alpha],
                part: [column[0], row[0], column[1], row[1]])
  end

  # An icon before a button's label, the two of them in its middle. Returns where the label's
  # middle is.
  def self.button_icon(icon, box, label, size)
    side = icon_side(box)
    before = side + padding / 2                       # the icon, and the room after it
    words = text_width(label, size: size)
    left = centered(box[:x], box[:width], before + words)
    image(icon, x: left, y: centered(box[:y], box[:height], side), width: side, height: side)
    left + before + (words / 2)
  end

  def self.icon_side(box) = (box[:height] * theme[:button, :icon_share]).to_i
end
