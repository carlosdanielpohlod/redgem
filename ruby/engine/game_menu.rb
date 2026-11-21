# A game's menus: a title screen, a pause menu (Esc, P or a gamepad's Start), an end screen, and a
# settings screen the first two lead to (see GameMenu::SettingsScreen), with its controls page
# (GameMenu::ControlsScreen). While a menu is open the
# game is frozen (Time.scale = 0); its options are buttons. Up and down (arrows, a gamepad's D-pad)
# move the focus between them, and Enter or a gamepad's A picks the focused one.
#
#   Camera.main.add_component(GameMenu)        # in a scene script's setup
#   GameMenu.instance.finish("You win!")       # from any script: the end screen
#
# Settings: title (shown on the title screen: the game's own title unless given), start_open
# (false starts the game right away), journal (false leaves the journal page off the pause menu of
# a game with quest files, see GameMenu::JournalScreen). On a dedicated server nobody is there to see a menu, and the
# game must never freeze: none opens. Its labels are the engine's texts (see I18n), looked up in the
# language in use each time they are drawn, so I18n.locale= relabels them; a game's own translation
# files may change them. A game's own label or heading may be a text, or a lookup that is drawn the
# same way: -> { I18n.t("menu.credits") }.
#
# The screens are :title, :pause, :over, :settings, :controls and :journal. A game changes them and adds its own:
#
#   menu.screens[:pause].add_option("Credits", index: 1) { menu.show(:credits) }
#   menu.screens[:credits] = GameMenu::Screen.new(heading: "Credits", back: -> { menu.show(:pause) })
#
# How it looks is its theme's (see game_menu_theme.rb and UI::Theme): game.json's "menu" section,
# GameMenu.theme = and each screen's own (screen.theme =). A game that draws its buttons or its
# background itself says so with on_draw_button and on_draw_background, here or on a screen.
class GameMenu < Behaviour
  PAUSE_KEYS = %i[escape p].freeze
  BACK_KEY = :escape
  PAUSE_BUTTON = :start
  RUNNING = 1.0
  FROZEN = 0.0
  LAST = -1                   # where an option is added unless said: after the others
  REFERENCE_HEIGHT = 720      # the interface's height (UI.height) the sizes of its theme are for
  REFERENCE_WIDTH = 800       # the width its widest rows need, with room beside them

  # The name of the open screen; nil while the game goes on.
  attr_reader :screen
  # Every screen by its name.
  attr_reader :screens

  def self.instance
    holder = Scene.objects.find { |object| object.get_component(GameMenu) }
    raise ArgumentError, "no object of the scene has a GameMenu" unless holder

    holder.get_component(GameMenu)
  end

  def initialize
    super
    @screen = nil
    @focus = UI::Focus.new
    @look = Look.new(self)
    @quit = [-> { I18n.t("game.menu.quit") }, -> { Game.quit }, { id: :quit }]
    @screens = { title: title_screen, pause: pause_screen(@quit), over: over_screen, **pages }
  end

  def start
    @screens[:title].heading = prop(:title, Game.title)
    add_journal if prop(:journal, Quests.any?)
    show(:title) if prop(:start_open, true)
  end

  def open? = !@screen.nil?

  # Whether any menu is open now: a game's HUD can stay out of its way (`return if GameMenu.open?`).
  def self.open? = !open_menus.empty?
  def self.open_menus = @open_menus ||= []

  # (A menu says it opened or closed: see GameMenu::Look.)
  def self.opened(menu) = open_menus.include?(menu) || open_menus << menu
  def self.closed(menu) = open_menus.delete(menu)

  # Opens a screen by its name and freezes the game.
  def show(name)
    return if Network.server?

    open_screen&.closed
    @screen = name
    Time.scale = FROZEN
    @focus.reset
    open_screen.focus = @focus
    open_screen.opened
    @look.shown(theme_of(name))
  end

  # Opens the settings screen, which goes back to the screen open now.
  def show_settings
    @screens[:settings].from = @screen
    show(:settings)
  end

  # Closes the menu: the game goes on.
  def resume
    open_screen&.closed
    @screen = nil
    Time.scale = RUNNING
    @look.left
  end

  # Starts the scene over.
  def restart
    Time.scale = RUNNING
    @look.left
    Game.load_scene(Game.scene_name)
  end

  # Ends the game with a message and offers to play again.
  def finish(message)
    @screens[:over].heading = message
    show(:over)
  end

  def update(_dt)
    go_back if pause_pressed?
    draw(open_screen) if open?
  end

  # (The camera and the picture are the game's again: the scene ends, or the menu goes.)
  def on_destroy = @look.left

  # Where the button at `index` of the open screen is: [x, y, width, height].
  def button_box(index)
    theme = theme_of(@screen)
    width = button_width(theme)
    height = scaled(theme[:button, :height])
    y = (UI.height * first_button_share).to_i + index * (height + scaled(theme[:button, :gap]))
    [column_x(width), y, width, height]
  end

  # How much larger than their sizes the menu's things are, in the interface's units (which grow
  # with the screen, see UI.scale): as the game's reference is taller than REFERENCE_HEIGHT, or as
  # the screen is, in units, when it is shorter or narrower than the menu needs (a player's large
  # Settings.ui_scale, a narrow window), so that the menu always fits: it already takes the
  # screen's height, so a larger interface size grows the game's HUD, not the menu. As they are
  # where there is no screen.
  def scale = [height_share, width_share].compact.min || 1.0

  # A size of the menu's, as it is drawn on this screen.
  def scaled(size) = (size * scale).to_i

  private

  # The menu's scale as the room in height allows (nil with no screen), and as the width does.
  def height_share
    room = [UI.reference[1], UI.height].select(&:positive?).min
    room && room.to_f / REFERENCE_HEIGHT
  end

  def width_share = UI.width.positive? ? UI.width.to_f / REFERENCE_WIDTH : nil

  # Where the first button sits: as the screen's own theme says, else as the screen does, else as
  # the menu's theme does.
  def first_button_share = own_top || open_screen&.first_button_share || theme_of(@screen)[:layout, :top]

  # The screens that are pages of their own.
  def pages
    { settings: SettingsScreen.new(self), controls: ControlsScreen.new(self), journal: JournalScreen.new(self) }
  end

  # The journal goes on the pause menu, before Quit (last, if the game took Quit away).
  def add_journal
    pause = @screens[:pause]
    pause.add_option(-> { I18n.t("game.menu.journal") }, index: pause.__index(@quit) || LAST,
                                                         id: :journal) { show(:journal) }
  end

  def settings_option = [-> { I18n.t("game.menu.settings") }, -> { show_settings }, { id: :settings }]

  def title_screen
    Screen.new(options: [[-> { I18n.t("game.menu.play") }, -> { resume }, { id: :play }],
                         settings_option, @quit])
  end

  def over_screen
    Screen.new(options: [[-> { I18n.t("game.menu.play_again") }, -> { restart }, { id: :play_again }], @quit])
  end

  def pause_screen(quit)
    options = [[-> { I18n.t("game.menu.resume") }, -> { resume }, { id: :resume }],
               [-> { I18n.t("game.menu.restart") }, -> { restart }, { id: :restart }], settings_option, quit]
    Screen.new(heading: -> { I18n.t("game.menu.paused") }, options: options, back: -> { resume })
  end

  def open_screen = @screens[@screen]

  # Letters are typed into the open screen: of the pause keys, only Esc is one there.
  def typing? = open? && open_screen.typing?

  def pause_keys = typing? ? [BACK_KEY] : PAUSE_KEYS

  def pause_pressed? = pause_keys.any? { |key| Input.pressed?(key) } || Input.gamepad_pressed?(PAUSE_BUTTON)

  # The pause keys open the pause menu while the game goes on; on a screen they do what it says.
  def go_back
    return show(:pause) unless open?

    back = open_screen.back
    @look.sound(theme_of(@screen), :back) if back
    back&.call
  end

  def draw(screen)
    UI.with_theme(theme_of(@screen)) do
      @look.backdrop(background_drawer(screen))
      @look.coming_in { draw_page(screen) }
    end
  end

  def draw_page(screen)
    draw_heading(screen)
    move_focus(screen.rows + screen.options.size)
    screen.draw(button_box(0))
    pick(clicked(screen))
  end

  # Up and down move the focus (with the theme's sound, when it moved).
  def move_focus(count)
    was = @focus.index
    @focus.update(count)
    @look.sound(UI.theme, :move) unless @focus.index == was
  end

  def pick(action)
    return unless action

    @look.sound(UI.theme, :pick)
    action.call
  end

  # On a screen letters are typed into, Enter is the text's: only a gamepad's A picks.
  def confirm_controls = typing? ? UI::GAMEPAD_CONFIRM : UI::CONFIRM

  # Draws the options as buttons, after the screen's rows in the focus; returns what the clicked
  # (or picked) one does, if one was.
  def clicked(screen)
    chosen = nil
    extras = screen.option_extras
    screen.options.each_with_index do |(label, action), index|
      chosen = action if button?(screen, label, index, extras[index] || {})
    end
    chosen
  end

  def button?(screen, label, index, extra)
    x, y, width, height = button_box(index)
    box = { x: x, y: y, width: width, height: height }
    focused = @focus.at?(screen.rows + index)
    disabled = Screen.disabled?(extra)
    draw_option(screen, label, box, UI.button_state(box, focused, disabled: disabled, id: extra[:id]))
    !disabled && UI.worked?(box, focused, confirm_controls)
  end
end
