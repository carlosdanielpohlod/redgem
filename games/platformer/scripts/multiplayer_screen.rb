# The pause menu's multiplayer screen: host a game, join one found on the same network with a
# click, or type an address (or, with a relay, a room's code) and join that.
# With a relay (see MainScene) games can also be hosted online: the others only need the code.
# With a dedicated server (see MainScene), Play online joins it: nobody has to host. It joins as
# the player logged in (Log in, with a backend) or as a guest (see Accounts).
class MultiplayerScreen < GameMenu::Screen
  NO_SERVER = I18n.t("multiplayer.no_server").freeze   # (in the language the game starts in)
  SERVER_KEY = :server              # the last address joined, kept between runs
  DEFAULT_SERVER = "127.0.0.1".freeze
  ROOM_CODE_LETTERS = 6
  STATUS_SIZE = 22
  STATUS_ABOVE_FIELD = 34           # pixels from the status line's top to the address field
  FIELD_ABOVE_BUTTONS = 74          # pixels from the address field's top to the first button
  HEIGHT_ABOVE_BUTTONS = FIELD_ABOVE_BUTTONS + STATUS_ABOVE_FIELD
  NEARBY_GAP = 40                   # pixels between the buttons and the games found nearby
  NEARBY_TITLE_SIZE = 20
  MAX_NEARBY = 4                    # games listed

  attr_reader :address
  # The port games are hosted on and joined at, and where Play online goes ("address" or "address:port").
  attr_accessor :port, :server

  # `menu` is the GameMenu it is a screen of; `title` is what a hosted game is called.
  def initialize(menu, title:, relay:, server:)
    super(heading: I18n.t("multiplayer.heading"), back: -> { menu.show(:pause) })
    @title = title
    @relay = relay.to_s
    @server = server.to_s
    @address = Save.get(SERVER_KEY, DEFAULT_SERVER).to_s
    @port = Network::DEFAULT_PORT
    @failed = false
    follow_the_network
  end

  # What the screen offers: [label, what it does].
  def options
    list = [[I18n.t("multiplayer.host"), -> { host_game }]]
    list << [I18n.t("multiplayer.host_online"), -> { host_room }] if relay?
    list << [I18n.t("multiplayer.log_in"), -> { GameMenu.instance.show(:login) }] if Accounts.backend?
    list + joining_options
  end

  def joining_options
    [[I18n.t("multiplayer.join"), -> { join_game }], [I18n.t("multiplayer.play_online"), -> { play_online }],
     [I18n.t("multiplayer.leave"), -> { leave_game }], [I18n.t("menu.back"), back]]
  end

  def opened = Network.search
  def closed = Network.stop_search
  def space_above = HEIGHT_ABOVE_BUTTONS
  def typing? = true   # the address: there P is a letter

  # Looks for games only while there is one to look for: offline, with the screen open.
  def keep_looking
    return Network.stop_search if in_or_joining_a_game?

    Network.search unless Network.searching?
  end

  # What is going on with the network, the address to join and the games found nearby, around
  # the buttons (`box` is where the first one is: [x, y, width, height]).
  def draw(box)
    x, y, width, height = box
    keep_looking
    UI.text(status, x: UI.width / 2, y: y - HEIGHT_ABOVE_BUTTONS, size: STATUS_SIZE, color: :gold, align: :center)
    @address = UI.text_field(@address, x: x, y: y - FIELD_ABOVE_BUTTONS, width: width, height: height)
    draw_nearby(x + width + NEARBY_GAP, y, width, height) unless Network.online?
  end

  def host_game
    starting
    Network.host(port: @port, name: @title)
  end

  def host_room
    starting
    Network.host(room: true)
  end

  def join_game
    starting(@address)
    Save[SERVER_KEY] = @address
    room_code? ? Network.join(room: @address) : Network.join(@address, port: @port)
  end

  # Joins the dedicated server, in any room with a free place.
  def play_online
    starting(@server)
    return @note = NO_SERVER if @server.empty?

    address, port = @server.split(":")
    Network.join(address, port: (port || Network::DEFAULT_PORT).to_i, auth: Accounts.auth)
  end

  def leave_game = Network.leave

  # What is going on with the network, in a line.
  def status
    return I18n.t("multiplayer.joining", address: @joining) if Network.connecting?
    return offline_status unless Network.online?

    others = Network.players.size
    return I18n.t("multiplayer.one_other", place: place) if others == 1

    I18n.t("multiplayer.others", place: place, count: others)
  end

  private

  # Where games meet online, and joins that fail.
  def follow_the_network
    Network.relay = @relay unless @relay.empty?
    Network.on_fail do |reason|
      @failed = true
      @reason = reason
    end
  end

  # Hosting or joining (`target`): an earlier failure is history, and there is nothing left to look for.
  def starting(target = @address)
    @failed = false
    @reason = nil
    @note = nil
    @joining = target
    Network.stop_search
  end

  def in_or_joining_a_game? = Network.online? || Network.connecting?

  def relay? = !@relay.empty?

  # A room's code is a few letters and digits; an address has dots (or is a longer name).
  def room_code? = relay? && @address.strip.size == ROOM_CODE_LETTERS && !@address.include?(".")

  def offline_status
    return @note if @note
    return failure if @failed

    hint
  end

  # Why joining failed, when it was said.
  def failure
    return I18n.t("multiplayer.join_failed", address: @joining) unless @reason

    I18n.t("multiplayer.join_failed_because", address: @joining, reason: @reason)
  end

  def hint = relay? ? I18n.t("multiplayer.hint_relay") : I18n.t("multiplayer.hint")

  def place
    return I18n.t("multiplayer.room", room: Network.room) if Network.room

    Network.host? ? I18n.t("multiplayer.hosting", port: Network.port) : I18n.t("multiplayer.in_game", address: @joining)
  end

  # The games hosted on this network, each a button that joins it.
  def draw_nearby(x, y, width, height)
    games = Network.games.first(MAX_NEARBY)
    title = games.empty? ? I18n.t("multiplayer.looking") : I18n.t("multiplayer.nearby")
    UI.text(title, x: x, y: y - STATUS_ABOVE_FIELD, size: NEARBY_TITLE_SIZE, color: :gray)
    games.each_with_index do |game, index|
      top = y + index * (height + GameMenu::BUTTON_GAP)
      join_found(game) if UI.button("#{game[:name]} (#{game[:players]})", x: x, y: top, width: width, height: height)
    end
  end

  def join_found(game)
    starting(game[:address])
    @address = game[:address]
    Network.join(game[:address], port: game[:port])
  end
end
