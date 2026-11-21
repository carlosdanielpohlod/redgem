# The menu's screen to play together (title and pause menus > Play together): host a game, type
# the host's address and join it, or join the game's dedicated server (Session::SERVER). The title
# screen's Play goes on alone from the save (Session.play). Esc goes back. What cannot start (a
# port taken, no server set) is said on the screen, which stays open; a join that fails later
# opens it again, saying why.
class LobbyScreen < GameMenu::Screen
  OPTION_INDEX = 1                  # on the title and pause screens: after Play or Resume
  DEFAULT_ADDRESS = "127.0.0.1".freeze
  STATUS_SIZE = 22
  STATUS_ABOVE_FIELD = 34           # pixels from the status line's top to the address field
  FIELD_ABOVE_BUTTONS = 74          # pixels from the address field's top to the first button

  # The address Join goes to; what the screen says (why a join failed, or that there is no server).
  attr_accessor :address
  attr_reader :note

  # Puts the screen in a GameMenu: as :together, offered by the title and the pause screens.
  def self.install(menu)
    lobby = LobbyScreen.new(menu)
    menu.screens[:title] = title_screen(menu)
    menu.screens[:together] = lobby
    %i[title pause].each do |from|
      menu.screens[from].add_option(-> { I18n.t("menu.together") }, index: OPTION_INDEX) { lobby.open_from(from) }
    end
    lobby
  end

  # The title screen the game opens on: Play (alone, from the save), Play together, Settings, Quit.
  def self.title_screen(menu)
    play = [-> { I18n.t("game.menu.play") }, lambda {
      menu.resume
      Session.play
    }]
    options = [play, [-> { I18n.t("game.menu.settings") }, -> { menu.show_settings }],
               [-> { I18n.t("game.menu.quit") }, -> { Game.quit }]]
    GameMenu::Screen.new(options: options)
  end

  def initialize(menu)
    super(heading: -> { I18n.t("lobby.heading") }, back: -> { menu.show(@from) })
    @menu = menu
    @address = DEFAULT_ADDRESS
    @from = :pause
    Network.on_fail { |reason| failed(reason) }
  end

  def options
    [[I18n.t("lobby.host"), -> { start(I18n.t("lobby.host_failed")) { Session.host } }],
     [I18n.t("lobby.join"), -> { start(I18n.t("lobby.join_failed")) { Session.join(@address) } }],
     [I18n.t("lobby.server"), -> { start(I18n.t("lobby.no_server")) { Session.play_on_server } }],
     [I18n.t("lobby.leave"), -> { Session.leave }], [I18n.t("lobby.back"), back]]
  end

  # Shows this screen; Back (and Esc) go back to `from`.
  def open_from(from)
    @from = from
    @menu.show(:together)
  end

  def space_above = FIELD_ABOVE_BUTTONS + STATUS_ABOVE_FIELD
  def typing? = true   # the address: there P is a letter

  # The status line and the address field, above the buttons (`box`: the first one's place).
  def draw(box)
    x, y, width, height = box
    UI.text(@note.to_s, x: UI.width / 2, y: y - space_above, size: STATUS_SIZE, color: :gold, align: :center)
    @address = UI.text_field(@address, x: x, y: y - FIELD_ABOVE_BUTTONS, width: width, height: height)
  end

  private

  # The game goes on once the network starts; the screen says `refusal` (a text) when it cannot.
  def start(refusal)
    @note = nil
    return @note = refusal unless yield

    @menu.resume
  end

  def failed(reason)
    @note = I18n.t("lobby.failed", reason: reason.to_s)
    @menu.show(:together)
  end
end
