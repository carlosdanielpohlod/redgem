# The theme UI draws with now (see UI::Theme), and what moves or fades all that is drawn for a
# while (a menu's screens as they come in: UI.shifted).
#
#   UI.theme = { colors: { button: :blue }, button: { radius: 0 } }   # every widget from now on
#   UI.theme[:colors, :button]                                         # => :blue
#   UI.with_theme(theme) { UI.button("Play", ...) }                   # with another, for a block
module UI
  NO_SHIFT = [0, 0, 1.0].freeze   # [x, y, alpha share]: drawn where it is said, as opaque as it says

  # The theme widgets are drawn with now: the one UI.with_theme gives, or the game's.
  def self.theme = themes.last || base_theme

  # The game's changes to the engine's look, for every widget (nil goes back to it). Raises an
  # ArgumentError that says what is wrong with a key or a value.
  def self.theme=(changes)
    @theme_layer = Theme.read!(changes)
    @base_theme = nil
    Theme.changed
  end

  # What UI.theme= changed (a layer, see UI::Theme), which GameMenu's themes start from.
  def self.theme_layer = @theme_layer ||= {}

  def self.base_theme = @base_theme ||= Theme.default.merged(theme_layer)

  # Draws what the block draws with a theme (a UI::Theme), its font (when it has one) included.
  def self.with_theme(theme)
    themes.push(theme)
    font_was = UI.font
    UI.font = theme[:font] if theme[:font]
    yield
  ensure
    themes.pop
    UI.font = font_was if theme[:font]
  end

  def self.themes = @themes ||= []

  # Draws what the block draws `x` and `y` units away, and at a share of its opacity (0 to 1):
  # the pointer is over it where it shows. Inside another, the two add up.
  def self.shifted(x: 0, y: 0, alpha: 1.0)
    dx, dy, share = shift
    shifts.push([dx + x, dy + y, share * alpha])
    yield
  ensure
    shifts.pop
  end

  def self.shifts = @shifts ||= []
  def self.shift = shifts.last || NO_SHIFT

  # An opacity, as UI.shifted has it now.
  def self.faded(alpha) = (alpha * shift[2]).to_i

  # A box's [x, y, width, height] where it is drawn, in whole units.
  def self.shifted_box(box)
    [(box[:x] + shift[0]).to_i, (box[:y] + shift[1]).to_i, box[:width].to_i, box[:height].to_i]
  end

  def self.shifted_point(x, y) = [x + shift[0], y + shift[1]]
end
