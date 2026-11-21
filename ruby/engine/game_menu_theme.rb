# How a game's menus look: each screen's theme (UI::Theme), made of the engine's look, the game's
# UI.theme, game.json's "menu" section, GameMenu.theme and the screen's own, each over the last:
#
#   GameMenu.theme = { heading_font: "Bangers", colors: { button: "#1d2b53", hover: :gold },
#                      layout: { align: :left, margins: [0, 0, 0, 64] } }
#   menu.screens[:title].theme = { background: { kind: :image, image: "title_art" },
#                                  logo: { image: "logo" }, layout: { heading: :none } }
#
# and where its heading and buttons go, as the theme lays them out (aligned left, in the middle or
# right, within its margins). A game that draws its buttons itself gives a block that draws one,
# for the menu or for one screen (a screen subclass may define draw_button instead):
#
#   menu.on_draw_button do |label, box, state|      # box: {x:, y:, width:, height:}; UI::ButtonState
#     UI.rect(**box, color: state.focused? ? :gold : :dark)
#     UI.text(label, x: box[:x] + 12, y: box[:y] + 8, size: 28)
#   end
#   menu.on_draw_background { UI.image(:paper, x: 0, y: 0, width: UI.width, height: UI.height) }
#
# The menu still hears the keys, a gamepad, the mouse and taps on them. (Experimental.)
class GameMenu < Behaviour
  MARGIN_SIDES = %i[top right bottom left].freeze   # the order of the theme's layout margins

  # The game's changes to how its menus look (a layer, see UI::Theme); nil takes them away.
  # Raises an ArgumentError that says what is wrong with a key or a value.
  def self.theme=(changes)
    @theme_layer = UI::Theme.read!(changes)
    UI::Theme.changed
  end

  def self.theme_layer = @theme_layer ||= {}

  # The menus' theme (a UI::Theme), before any screen's own.
  def self.theme = UI.base_theme.merged(game_menu[:theme]).merged(theme_layer)

  # game.json's "menu" section, read once: {theme: layer, screens: {name => layer}}. What cannot be
  # used of it is warned of, and left out.
  def self.game_menu = @game_menu ||= read_game_menu

  def self.read_game_menu
    menu, problems = UI::Theme.read_menu(UI.__menu_settings || {})
    problems.each { |problem| Debug.warn("game.json: menu: #{problem}") }
    menu
  end

  # Draws the menu's buttons with a block (|label, box, state|) instead of the theme's look.
  def on_draw_button(&block)
    @button_drawer = block
  end

  # Draws the menu's background with a block instead of the theme's.
  def on_draw_background(&block)
    @background_drawer = block
  end

  # The theme a screen is drawn with (the menus' for none), made again only when a layer changed.
  def theme_of(name)
    @themes ||= {}
    kept = @themes[name]
    return kept.last if kept && kept.first == UI::Theme.version

    theme = GameMenu.theme.merged(own_layer(name))
    @themes[name] = [UI::Theme.version, theme]
    theme
  end

  # Where a box `width` wide starts across the screen, as the theme aligns the menu.
  def column_x(width)
    left, right = side_margins
    case UI.theme_of_menu(self)[:layout, :align]
    when :left then left
    when :right then UI.width - right - width
    else left + (UI.width - left - right - width) / 2
    end
  end

  # Where texts aligned with the menu are drawn from: [x, align].
  def column_text_x
    left, right = side_margins
    align = UI.theme_of_menu(self)[:layout, :align]
    x = { left: left, right: UI.width - right }.fetch(align) { left + (UI.width - left - right) / 2 }
    [x, align]
  end

  # One of the theme's margins (:top, :right, :bottom or :left), on this screen.
  def margin(side) = scaled(UI.theme_of_menu(self)[:layout, :margins][MARGIN_SIDES.index(side)])

  private

  def side_margins = [margin(:left), margin(:right)]

  # What a screen's own theme says over the menus': game.json's, then the screen's.
  def own_layer(name)
    from_file = GameMenu.game_menu[:screens][name.to_s] || {}
    UI::Theme.merge(from_file, @screens[name]&.theme_layer || {})
  end

  def own_top = own_layer(@screen).fetch(:layout, {})[:top]

  # As wide as the theme's buttons, or (with a max_width) as the widest label needs, up to it.
  def button_width(theme)
    width = scaled(theme[:button, :width])
    most = theme[:button, :max_width]
    return width unless most && open?

    needed = widest_label(theme) + 2 * scaled(theme[:button, :padding])
    needed.clamp(width, [scaled(most), width].max)
  end

  def widest_label(theme)
    size = (scaled(theme[:button, :height]) * theme[:sizes, :label_share]).to_i
    open_screen.options.map { |label, _| UI.text_width(label, size: size) }.max || 0
  end

  def draw_heading(screen)
    theme = UI.theme
    place = theme[:layout, :heading]
    return if place == :none

    x, align = column_text_x
    UI.with_font(theme[:heading_font]) do
      UI.text(screen.heading, x: x, y: heading_y(screen, place, theme), size: scaled(theme[:sizes, :heading]),
                              color: theme[:colors, :heading], align: align)
    end
  end

  def heading_y(screen, place, theme)
    return margin(:top) + scaled(theme[:layout, :heading_top]) if place == :top

    button_box(0)[1] - screen.space_above - scaled(theme[:layout, :heading_gap])
  end

  # An option's button: the screen's drawer, the menu's, or the theme's look (with its icon).
  def draw_option(screen, label, box, state)
    drawer = screen.button_drawer || button_drawer
    return drawer.call(label, box, state) if drawer

    UI.draw_button(label, box, state, icon: icon_of(state.id))
  end

  def icon_of(id) = id && UI.theme[:button, :icons][id.to_s]

  def button_drawer = @button_drawer || (->(*drawn) { draw_button(*drawn) } if respond_to?(:draw_button))

  def background_drawer(screen)
    screen.background_drawer || @background_drawer || (-> { draw_background } if respond_to?(:draw_background))
  end
end

# (What the menu draws with, in UI.)
module UI # rubocop:disable Style/OneClassPerFile
  # The theme the menu lays itself out with: the one it is drawn with while it draws, else its open
  # screen's (as a script asks where a button is, between frames).
  def self.theme_of_menu(menu) = themes.empty? ? menu.theme_of(menu.screen) : theme

  # Draws what the block draws in a font (UI.font), and goes back to the one before; nil keeps it.
  def self.with_font(font)
    return yield if font.nil?

    font_was = UI.font
    UI.font = font
    yield
  ensure
    UI.font = font_was if font
  end
end
