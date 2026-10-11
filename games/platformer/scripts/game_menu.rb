# The game's menus: a title screen, a pause menu (Esc, P or a gamepad's Start) and an end screen. While a menu is
# open the game is frozen (Time.scale = 0); menus are drawn and clicked with UI.button, and a
# gamepad's A picks the first option.
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
  PAUSE_KEYS = [:escape, :p].freeze
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

  def self.instance
    Scene.objects.each do |object|
      menu = object.get_component(GameMenu)
      return menu if menu
    end
    nil
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
    # (While an address is being typed, P is a letter.)
    keys = @screen == :multiplayer ? [BACK_KEY] : PAUSE_KEYS
    toggle_pause if keys.any? { |key| Input.pressed?(key) } || Input.gamepad_pressed?(PAUSE_BUTTON)
    return unless @screen

    draw
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

  def toggle_pause
    if @screen == :pause
      resume
    elsif @screen == :multiplayer
      back
    elsif @screen.nil?
      show(:pause)
    end
  end

  def draw
    UI.rect(x: 0, y: 0, width: UI.width, height: UI.height, color: :black, alpha: SHADE_ALPHA)
    heading = HEADINGS[@screen] || @heading
    # The multiplayer screen has a status line and the address field between its heading and buttons.
    top = button_box(0)[1] - (@screen == :multiplayer ? MultiplayerScreen::HEIGHT_ABOVE_BUTTONS : 0)
    UI.text(heading, x: UI.width / 2, y: top - TITLE_ABOVE_BUTTONS, size: TITLE_SIZE, align: :center)
    @multiplayer.draw(button_box(0)) if @screen == :multiplayer
    chosen = options.first.last if Input.gamepad_pressed?(CONFIRM_BUTTON)
    options.each_with_index do |(label, action), index|
      x, y, width, height = button_box(index)
      chosen = action if UI.button(label, x: x, y: y, width: width, height: height)
    end
    choose(chosen) if chosen
  end

  # What the open screen offers: [label, what it does].
  def options = @screen == :multiplayer ? @multiplayer.options : SCREENS[@screen]

  # The multiplayer screen does its own things; the rest is this menu's.
  def choose(action)
    @screen == :multiplayer && @multiplayer.respond_to?(action) ? @multiplayer.__send__(action) : __send__(action)
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
