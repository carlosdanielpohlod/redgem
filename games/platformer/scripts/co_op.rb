# Playing together over the network: every machine in the game runs its own hero, sees the
# others' and shares the coins. Whoever reaches the flag with every coin collected wins for all.
# Start or join a game from the pause menu (Esc > Multiplayer). On a dedicated server the game runs
# with no hero of its own: it keeps the coins and the win for everyone, and when the game is won it
# ends the match, so that the next players get a new one. Lives on the GameManager.
class CoOp < Behaviour
  HERO = "Player".freeze
  HERO_KEY_PREFIX = "hero-".freeze   # a hero is called this on the network, followed by its machine's id
  # What the other players' heroes look like, by turns.
  TINTS = [[0.45, 0.75, 1.0], [0.55, 1.0, 0.6], [1.0, 0.6, 0.9], [1.0, 0.9, 0.5]].freeze
  COIN = :coin              # a coin was picked up: its name
  COINS_GONE = :coins_gone  # to a newcomer: the names of the coins picked up before it arrived
  WON = :won
  MATCH_OVER_SECONDS = 5.0  # after a win, on a dedicated server: then its players are let go

  attr_reader :ghosts

  def self.instance = GameManager.instance.get_component(CoOp)

  def start
    @ghosts = {}
    follow_the_players
    follow_the_game
    enter if Network.online?
  end

  # This machine's hero picked a coin up, or won: the others hear of it.
  def collected(name) = Network.emit(COIN, name)
  def won = Network.emit(WON)

  private

  # Who comes into the game and who leaves it.
  def follow_the_players
    Network.on_connect { enter }
    Network.on_join { |peer| welcome(peer) }
    Network.on_leave { |peer| @ghosts.delete(peer)&.destroy }
    Network.on_disconnect { leave }
  end

  # What the other players' heroes did.
  def follow_the_game
    Network.on(COIN) { |name, _from| take(name) }
    Network.on(COINS_GONE) { |names, _from| names.each { |name| take(name) } }
    Network.on(WON) { won_by_another }
  end

  def won_by_another
    GameManager.instance.win(mine: false)
    after(MATCH_OVER_SECONDS) { Game.quit } if Network.server?
  end

  def hero_key(peer) = "#{HERO_KEY_PREFIX}#{peer}"

  # In a game: this machine's hero becomes visible to the others, and theirs appear here.
  def enter
    show_my_hero unless Network.server?
    Network.players.each { |peer| add_ghost(peer) }
  end

  def show_my_hero
    hero = find(HERO)
    sync = hero.get_component(NetworkSync) || hero.add_component(NetworkSync)
    sync.assign(owner: Network.id, key: hero_key(Network.id))
  end

  def welcome(peer)
    add_ghost(peer)
    Network.emit(COINS_GONE, GameManager.instance.collected, to: peer) if Network.host?
  end

  # Another player's hero: a copy of ours that goes where their machine says.
  def add_ghost(peer)
    return if @ghosts[peer]

    ghost = Scene.spawn(HERO, physics: false)  # seen, not bumped into
    ghost.tint = TINTS[peer % TINTS.size]
    ghost.add_component(NetworkSync, owner: peer, key: hero_key(peer))
    @ghosts[peer] = ghost
  end

  def take(name)
    coin = Scene.find(name)
    return unless coin  # already picked up here

    coin.get_component(Coin).collect
  end

  def leave
    @ghosts.each_value(&:destroy)
    @ghosts = {}
  end
end
