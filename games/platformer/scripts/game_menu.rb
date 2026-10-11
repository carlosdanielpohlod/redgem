# The game's menus: a title screen, a pause menu (Esc, P or a gamepad's Start) and an end screen.
# While a menu is open the game is frozen (Time.scale = 0); menus are drawn and clicked with
# UI.button, and a gamepad's A picks the first option.
# The pause menu also leads to the multiplayer screen (see MultiplayerScreen).
# Other scripts end the game with: GameMenu.instance.finish("You win!")
# Settings: title (shown on the title screen), start_open (false starts the game right away),
# relay (where games meet over the internet, if anywhere).
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
  CONFIRM_BUTTON = :a
  RUNNING = 1.0
  FROZEN = 0.0

  # What each screen offers: [label, what it does].
  SCREENS = {
    title: [["Play", :resume], ["Quit", :quit]],
    pause: [["Resume", :resume], ["Restart", :restart], ["Multiplayer", :multiplayer_screen], ["Quit", :quit]],
    over: [["Play again", :restart], ["Quit", :quit]]
  }.freeze
  HEADINGS = { pause: "Paused", multiplayer: "Multiplayer" }.freeze

  # What the pause keys do on each screen (nothing on the title and end screens).
  PAUSE_TOGGLES = { nil => :pause, :pause => :resume, :multiplayer => :back }.freeze

  def self.instance
    holder = Scene.objects.find { |object| object.get_component(GameMenu) }
    raise ArgumentError, "no object of the scene has a GameMenu" unless holder

    holder.get_component(GameMenu)
  end

  def start
    @heading = prop(:title, "My Game")
    @screen = nil
    @multiplayer = MultiplayerScreen.new(title: @heading, relay: prop(:relay, nil))
    show(:title) if prop(:start_open, true)
  end

  attr_reader :screen, :multiplayer

  def open? = !@screen.nil?

  # Ends the game with a message and offers to play again.
  def finish(message)
    @heading = message
    show(:over)
  end

  def update(_dt)
    toggle_pause if pause_pressed?
    draw if @screen
  end

  # Where the button at `index` of the open screen is: [x, y, width, height].
  def button_box(index)
    x = (UI.width - BUTTON_WIDTH) / 2
    y = (UI.height * FIRST_BUTTON_SHARE).to_i + index * (BUTTON_HEIGHT + BUTTON_GAP)
    [x, y, BUTTON_WIDTH, BUTTON_HEIGHT]
  end

  private

  def show(screen)
    @screen = screen
    Time.scale = FROZEN
  end

  def multiplayer? = @screen == :multiplayer

  # (While an address is being typed, P is a letter.)
  def pause_keys = multiplayer? ? [BACK_KEY] : PAUSE_KEYS

  def pause_pressed? = pause_keys.any? { |key| Input.pressed?(key) } || Input.gamepad_pressed?(PAUSE_BUTTON)

  def toggle_pause
    toggle = PAUSE_TOGGLES[@screen]
    __send__(toggle) if toggle
  end

  def pause = show(:pause)

  def draw
    UI.rect(x: 0, y: 0, width: UI.width, height: UI.height, color: :black, alpha: SHADE_ALPHA)
    UI.text(HEADINGS[@screen] || @heading, x: UI.width / 2, y: heading_y, size: TITLE_SIZE, align: :center)
    @multiplayer.draw(button_box(0)) if multiplayer?
    chosen = draw_buttons || confirmed_option
    choose(chosen) if chosen
  end

  # The multiplayer screen has a status line and the address field between its heading and buttons.
  def heading_y
    first_button_y = button_box(0)[1]
    first_button_y - (multiplayer? ? MultiplayerScreen::HEIGHT_ABOVE_BUTTONS : 0) - TITLE_ABOVE_BUTTONS
  end

  # Draws the open screen's buttons; returns what the clicked one does, if one was.
  def draw_buttons
    clicked = nil
    options.each_with_index { |(label, action), index| clicked = action if button?(label, index) }
    clicked
  end

  def button?(label, index)
    x, y, width, height = button_box(index)
    UI.button(label, x: x, y: y, width: width, height: height)
  end

  # A gamepad's A picks the first option.
  def confirmed_option
    options.first.last if Input.gamepad_pressed?(CONFIRM_BUTTON)
  end

  # What the open screen offers: [label, what it does].
  def options = multiplayer? ? @multiplayer.options : SCREENS[@screen]

  # The multiplayer screen does its own things; the rest is this menu's.
  def choose(action)
    multiplayer? && @multiplayer.respond_to?(action) ? @multiplayer.__send__(action) : __send__(action)
  end

  def resume
    @multiplayer.closed
    @screen = nil
    Time.scale = RUNNING
  end

  def multiplayer_screen
    @screen = :multiplayer
    @multiplayer.opened
  end

  def back
    @multiplayer.closed
    @screen = :pause
  end

  def restart
    Time.scale = RUNNING
    Game.load_scene(Game.scene_name)
  end

  def quit = Game.quit
end
