# The game's menus: a title screen, a pause menu (Esc, P or a gamepad's Start) and an end screen.
# While a menu is open the game is frozen (Time.scale = 0); menus are drawn and clicked with
# UI.button, and a gamepad's A picks the first option.
# Other scripts end the game with: GameMenu.instance.finish("You win!")
# Settings: title (shown on the title screen), start_open (false starts the game right away).
class GameMenu < Behaviour
  BUTTON_WIDTH = 280
  BUTTON_HEIGHT = 58
  BUTTON_GAP = 16
  TITLE_SIZE = 72
  TITLE_ABOVE_BUTTONS = 130   # pixels from the title's top to the first button
  FIRST_BUTTON_SHARE = 0.46   # where the first button sits, as a share of the screen's height
  SHADE_ALPHA = 170           # how dark the game gets behind a menu
  PAUSE_KEYS = %i[escape p].freeze
  PAUSE_BUTTON = :start
  CONFIRM_BUTTON = :a
  RUNNING = 1.0
  FROZEN = 0.0

  # What each screen offers: [label, what it does].
  SCREENS = {
    title: [["Play", :resume], ["Quit", :quit]],
    pause: [["Resume", :resume], ["Restart", :restart], ["Quit", :quit]],
    over: [["Play again", :restart], ["Quit", :quit]]
  }.freeze
  HEADINGS = { pause: "Paused" }.freeze

  # What the pause keys do on each screen (nothing on the title and end screens).
  PAUSE_TOGGLES = { nil => :pause, :pause => :resume }.freeze

  def self.instance
    holder = Scene.objects.find { |object| object.get_component(GameMenu) }
    raise ArgumentError, "no object of the scene has a GameMenu" unless holder

    holder.get_component(GameMenu)
  end

  def start
    @heading = prop(:title, "My Game")
    @screen = nil
    show(:title) if prop(:start_open, true)
  end

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

  def pause_pressed? = PAUSE_KEYS.any? { |key| Input.pressed?(key) } || Input.gamepad_pressed?(PAUSE_BUTTON)

  def toggle_pause
    toggle = PAUSE_TOGGLES[@screen]
    __send__(toggle) if toggle
  end

  def pause = show(:pause)

  def draw
    UI.rect(x: 0, y: 0, width: UI.width, height: UI.height, color: :black, alpha: SHADE_ALPHA)
    first_button_y = button_box(0)[1]
    UI.text(HEADINGS[@screen] || @heading, x: UI.width / 2, y: first_button_y - TITLE_ABOVE_BUTTONS,
                                           size: TITLE_SIZE, align: :center)
    chosen = draw_buttons || confirmed_option
    __send__(chosen) if chosen
  end

  # What the open screen offers: [label, what it does].
  def options = SCREENS[@screen]

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

  def resume
    @screen = nil
    Time.scale = RUNNING
  end

  def restart
    Time.scale = RUNNING
    Game.load_scene(Game.scene_name)
  end

  def quit = Game.quit
end
