# The pause menu's multiplayer screen: host a game, join one found on the same network with a
# click, or type an address (or, with a relay, a room's code) and join that.
# With a relay (see MainScene) games can also be hosted online: the others only need the code.
class MultiplayerScreen < GameMenu::Screen
  HEADING = "Multiplayer".freeze
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
  attr_accessor :port               # the port games are hosted on and joined at

  # `menu` is the GameMenu it is a screen of; `title` is what a hosted game is called.
  def initialize(menu, title:, relay:)
    super(heading: HEADING, back: -> { menu.show(:pause) })
    @title = title
    @relay = relay.to_s
    @address = Save.get(SERVER_KEY, DEFAULT_SERVER).to_s
    @port = Network::DEFAULT_PORT
    @failed = false
    Network.relay = @relay unless @relay.empty?
    Network.on_fail { @failed = true }
  end

  # What the screen offers: [label, what it does].
  def options
    list = [["Host a game", -> { host_game }]]
    list << ["Host online", -> { host_room }] if relay?
    list + [["Join", -> { join_game }], ["Leave the game", -> { leave_game }], ["Back", back]]
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
    starting
    Save[SERVER_KEY] = @address
    room_code? ? Network.join(room: @address) : Network.join(@address, port: @port)
  end

  def leave_game = Network.leave

  private

  # Hosting or joining: an earlier failure is history, and there is nothing left to look for.
  def starting
    @failed = false
    Network.stop_search
  end

  def in_or_joining_a_game? = Network.online? || Network.connecting?

  def relay? = !@relay.empty?

  # A room's code is a few letters and digits; an address has dots (or is a longer name).
  def room_code? = relay? && @address.strip.size == ROOM_CODE_LETTERS && !@address.include?(".")

  def status
    return "Joining #{@address}..." if Network.connecting?
    return @failed ? "Could not join #{@address}" : hint unless Network.online?

    others = Network.peers.size
    "#{place}: #{others} other #{others == 1 ? "player" : "players"}"
  end

  def hint = relay? ? "Type a room's code or the host's address" : "Type the host's address to join a game"

  def place
    return "Room #{Network.room}" if Network.room

    Network.host? ? "Hosting on port #{Network.port}" : "In the game at #{@address}"
  end

  # The games hosted on this network, each a button that joins it.
  def draw_nearby(x, y, width, height)
    games = Network.games.first(MAX_NEARBY)
    title = games.empty? ? "Looking for games nearby..." : "Games nearby"
    UI.text(title, x: x, y: y - STATUS_ABOVE_FIELD, size: NEARBY_TITLE_SIZE, color: :gray)
    games.each_with_index do |game, index|
      top = y + index * (height + GameMenu::BUTTON_GAP)
      join_found(game) if UI.button("#{game[:name]} (#{game[:players]})", x: x, y: top, width: width, height: height)
    end
  end

  def join_found(game)
    starting
    @address = game[:address]
    Network.join(game[:address], port: game[:port])
  end
end
