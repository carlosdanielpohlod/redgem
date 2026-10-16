# Playing online on a dedicated server (a machine running `redgem-server` on this game): everyone
# who joins it runs around the same level and sees the others' heroes. Put the server's address
# in SERVER ("play.example.com", or "play.example.com:7777"); the title and pause menus offer
# "Play online". The server runs this game too, with no hero of its own (Network.server?).
class Online < Behaviour
  SERVER = "".freeze
  HERO = "Player".freeze
  KEY_PREFIX = "hero-".freeze   # a hero's name on the network, followed by its machine's id
  OPTION_INDEX = 1              # on the title and pause screens: after Play or Resume
  NO_SERVER = I18n.t("online.no_server").freeze   # (in the language the game starts in)
  NOTICE_SECONDS = 5.0
  NOTICE_X = 20
  NOTICE_Y = 60
  NOTICE_SIZE = 22

  attr_accessor :server
  attr_reader :others, :notice

  def start
    @server = SERVER
    @others = {}
    follow_the_players
    offer_in(GameMenu.instance)
  end

  # Joins the server, in any room with a free place.
  def play
    GameMenu.instance.resume
    return tell(NO_SERVER) if @server.empty?

    address, port = @server.split(":")
    Network.join(address, port: (port || Network::DEFAULT_PORT).to_i)
    tell(I18n.t("online.joining", server: @server))
  end

  def update(_dt)
    UI.text(@notice, x: NOTICE_X, y: NOTICE_Y, size: NOTICE_SIZE, color: :gold) if Time.elapsed < @notice_until.to_f
  end

  private

  def offer_in(menu)
    label = I18n.t("online.play")
    %i[title pause].each { |screen| menu.screens[screen].add_option(label, index: OPTION_INDEX) { play } }
  end

  def follow_the_players
    Network.on_connect { enter }
    Network.on_fail { tell(I18n.t("online.unreachable", server: @server)) }
    Network.on_join { |peer| add_other(peer) }
    Network.on_leave { |peer| @others.delete(peer)&.destroy }
    Network.on_disconnect { leave }
  end

  # In the game: this machine's hero becomes visible to the others, and theirs appear here.
  def enter
    show_my_hero unless Network.server?
    Network.players.each { |peer| add_other(peer) }
    tell(I18n.t("online.in_room", room: Network.room))
  end

  def show_my_hero
    hero = find!(HERO)
    sync = hero.get_component(NetworkSync) || hero.add_component(NetworkSync)
    sync.assign(owner: Network.id, key: "#{KEY_PREFIX}#{Network.id}")
  end

  # Another player's hero: a copy of ours that goes where their machine says.
  def add_other(peer)
    @others[peer] ||= Scene.spawn(HERO, physics: false).tap do |hero|
      hero.add_component(NetworkSync, owner: peer, key: "#{KEY_PREFIX}#{peer}")
    end
  end

  def leave
    @others.each_value(&:destroy)
    @others = {}
    tell(I18n.t("online.over"))
  end

  def tell(text)
    @notice = text
    @notice_until = Time.elapsed + NOTICE_SECONDS
  end
end
