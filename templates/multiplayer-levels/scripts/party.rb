# The players of a game played together, in the level that runs now. This machine's hero is put
# on its spawn point and shown to the others (a NetworkSync it owns); each other player's hero is
# a copy of the prefab "Teammate" that goes where that player's machine says. A level that loads
# starts a new Party: every machine places its hero again and makes the others' copies again.
class Party < Behaviour
  HERO = "Player".freeze
  TEAMMATE = "Teammate".freeze
  KEY_PREFIX = "hero-".freeze   # a hero's name on the network, followed by its machine's id

  # The other players' heroes, by their machines' ids.
  attr_reader :teammates

  def self.instance = Camera.main.get_component(Party)

  def initialize
    super
    @teammates = {}
  end

  def start
    follow_the_players
    place_my_hero
    enter if Network.online?
  end

  private

  def follow_the_players
    Network.on_connect { enter }
    Network.on_join { |peer| add_teammate(peer) }
    Network.on_leave { |peer| @teammates.delete(peer)&.destroy }
    Network.on_disconnect { leave }
  end

  # (A dedicated server has no hero.)
  def my_hero = Network.server? ? nil : find(HERO)

  def place_my_hero
    hero = my_hero
    Spawns.place(hero) if hero
  end

  # In a game: this machine's hero goes to its own spawn point and is shown to the others.
  def enter
    show_my_hero
    Network.players.each { |peer| add_teammate(peer) }
  end

  def show_my_hero
    hero = my_hero
    return unless hero

    Spawns.place(hero)
    sync = hero.get_component(NetworkSync) || hero.add_component(NetworkSync)
    sync.assign(owner: Network.id, key: "#{KEY_PREFIX}#{Network.id}")
  end

  def add_teammate(peer)
    return if @teammates[peer] || peer == Network.id

    teammate = Scene.spawn(TEAMMATE, physics: false, name: "#{KEY_PREFIX}#{peer}")
    teammate.add_component(NetworkSync, owner: peer, key: "#{KEY_PREFIX}#{peer}")
    @teammates[peer] = teammate
  end

  # The game is over (the host left, or this machine did): playing alone again.
  def leave
    @teammates.each_value(&:destroy)
    @teammates = {}
    place_my_hero
  end
end
