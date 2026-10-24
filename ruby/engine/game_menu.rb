# A game's menus: a title screen, a pause menu (Esc, P or a gamepad's Start), an end screen, and a
# settings screen the first two lead to (see GameMenu::SettingsScreen). While a menu is open the
# game is frozen (Time.scale = 0); its options are buttons. Up and down (arrows, a gamepad's D-pad)
# move the focus between them, and Enter or a gamepad's A picks the focused one.
#
#   Camera.main.add_component(GameMenu)        # in a scene script's setup
#   GameMenu.instance.finish("You win!")       # from any script: the end screen
#
# Settings: title (shown on the title screen: the game's own title unless given), start_open
# (false starts the game right away). On a dedicated server nobody is there to see a menu, and the
# game must never freeze: none opens. Its labels are the engine's texts (see I18n), looked up in the
# language in use each time they are drawn, so I18n.locale= relabels them; a game's own translation
# files may change them. A game's own label or heading may be a text, or a lookup that is drawn the
# same way: -> { I18n.t("menu.credits") }.
#
# The screens are :title, :pause and :over. A game changes them and adds its own:
#
#   menu.screens[:pause].add_option("Credits", index: 1) { menu.show(:credits) }
#   menu.screens[:credits] = GameMenu::Screen.new(heading: "Credits", back: -> { menu.show(:pause) })
class GameMenu < Behaviour
  BUTTON_WIDTH = 280
  BUTTON_HEIGHT = 58
  BUTTON_GAP = 16
  TITLE_SIZE = 72
  TITLE_ABOVE_BUTTONS = 130   # pixels from the title's top to the first button
  FIRST_BUTTON_SHARE = 0.46   # where the first button sits, as a share of the screen's height
  SHADE_ALPHA = 170           # how dark the game gets behind a menu
  PAUSE_KEYS = %i[escape p].freeze
  BACK_KEY = :escape
  PAUSE_BUTTON = :start
  RUNNING = 1.0
  FROZEN = 0.0
  LAST = -1                   # where an option is added unless said: after the others
  REFERENCE_HEIGHT = 720      # the screen height the sizes above are for: a shorter one shrinks them

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
    quit = [-> { I18n.t("game.menu.quit") }, -> { Game.quit }]
    title = [[-> { I18n.t("game.menu.play") }, -> { resume }], settings_option, quit]
    @screens = { title: Screen.new(options: title), pause: pause_screen(quit),
                 over: Screen.new(options: [[-> { I18n.t("game.menu.play_again") }, -> { restart }], quit]),
                 settings: SettingsScreen.new(self) }
  end

  def start
    @screens[:title].heading = prop(:title, Game.title)
    show(:title) if prop(:start_open, true)
  end

  def open? = !@screen.nil?

  # Opens a screen by its name and freezes the game.
  def show(name)
    return if Network.server?

    open_screen&.closed
    @screen = name
    Time.scale = FROZEN
    @focus.reset
    open_screen.focus = @focus
    open_screen.opened
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
  end

  # Starts the scene over.
  def restart
    Time.scale = RUNNING
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

  # Where the button at `index` of the open screen is: [x, y, width, height].
  def button_box(index)
    width = scaled(BUTTON_WIDTH)
    height = scaled(BUTTON_HEIGHT)
    y = (UI.height * first_button_share).to_i + index * (height + scaled(BUTTON_GAP))
    [(UI.width - width) / 2, y, width, height]
  end

  # How much smaller than their sizes the menu's things are drawn: on a screen shorter than
  # REFERENCE_HEIGHT, as much shorter (never larger; as they are where there is no screen).
  def scale = UI.height.positive? ? [UI.height.to_f / REFERENCE_HEIGHT, 1.0].min : 1.0

  # A size of the menu's, as it is drawn on this screen.
  def scaled(size) = (size * scale).to_i

  private

  def first_button_share = open_screen&.first_button_share || FIRST_BUTTON_SHARE

  def settings_option = [-> { I18n.t("game.menu.settings") }, -> { show_settings }]

  def pause_screen(quit)
    options = [[-> { I18n.t("game.menu.resume") }, -> { resume }], [-> { I18n.t("game.menu.restart") }, -> { restart }],
               settings_option, quit]
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

    open_screen.back&.call
  end

  def draw(screen)
    UI.rect(x: 0, y: 0, width: UI.width, height: UI.height, color: :black, alpha: SHADE_ALPHA)
    draw_heading(screen)
    @focus.update(screen.rows + screen.options.size)
    screen.draw(button_box(0))
    clicked(screen.options, screen.rows)&.call
  end

  # On a screen letters are typed into, Enter is the text's: only a gamepad's A picks.
  def confirm_controls = typing? ? UI::GAMEPAD_CONFIRM : UI::CONFIRM

  def draw_heading(screen)
    heading_y = button_box(0)[1] - screen.space_above - scaled(TITLE_ABOVE_BUTTONS)
    UI.text(screen.heading, x: UI.width / 2, y: heading_y, size: scaled(TITLE_SIZE), align: :center)
  end

  # Draws the options as buttons, after the screen's `rows` in the focus; returns what the clicked
  # (or picked) one does, if one was.
  def clicked(options, rows)
    chosen = nil
    options.each_with_index { |(label, action), index| chosen = action if button?(label, index, rows + index) }
    chosen
  end

  def button?(label, index, place)
    x, y, width, height = button_box(index)
    UI.button(label, x: x, y: y, width: width, height: height, focused: @focus.at?(place), confirm: confirm_controls)
  end
end
