# Multiplayer: one machine hosts a game and the others join it, over the local network or the
# internet. Machines talk in named messages; each has an id (the host is 1).
#
#   Network.host                        # on one machine ...
#   Network.join("192.168.0.12")        # ... and on the others, by its address,
#   Network.search; Network.games       # or found by itself on the same network.
#
# Over the internet, where machines cannot reach each other directly, they meet on a relay:
#
#   Network.relay = "relay.example.com"
#   Network.host(room: true)            # Network.room is then a code like "K7QM3P" ...
#   Network.join(room: "K7QM3P")          # ... which is all the others need
#
# Or on a dedicated server (redgem-server), where the game runs with nobody at it and everyone joins:
#
#   Network.join("play.example.com")                  # any room with a free place
#   Network.join("play.example.com", room: "K7QM3P")  # the room a friend is in (their Network.room)
#   Network.server?                     # true on the server itself, which plays nobody
#   Network.players                     # the others who play: Network.peers without the server
#
# A join may carry what the game joined decides on (a session token from a login service), and
# that game decides who comes in (see network_approval.rb):
#
#   Network.join("play.example.com", auth: { token: token })
#   Network.authenticate { |peer, auth, decide| decide.accept(identity: ...) or decide.deny("why") }
#
#   Network.on(:hit) { |data, from| @health -= data[:damage] }
#   Network.emit(:hit, { damage: 3 })   # to everyone else; to: :host or to: a peer's id for one
#
# A message carries nil, true/false, numbers, texts, Vector3, and arrays and hashes of those.
# Messages arrive whole and in the order they were sent. What Network.on and the on_... calls
# register lasts until the next level is loaded.
module Network
  DEFAULT_PORT = 7777
  DEFAULT_RELAY_PORT = 7778
  DISCOVERY_PORT = 7776   # where games on a local network ask each other who is hosting
  WAITING_PER_ADDRESS = 4 # machines from one address that may wait to be let in at once (at most 32)
  EVERYONE = 0
  HOST = 1
  LOCAL = 0   # the owner of something made while this machine was in no game
  MAX_SPAWNS_PER_PEER = 64   # objects one machine may have spawned at a time
  MAX_SPAWNS = 512           # ... and all of them together

  # What the engine's own messages are called. Games' names never start with "$".
  RESERVED = "$".freeze
  SYNC = "$sync".freeze          # where an object is: [key, position, rotation]
  SPAWN = "$spawn".freeze        # an object to create: [key, template, owner, position, rotation]
  SPAWNED = "$spawned".freeze    # the host to a newcomer: every object spawned so far
  DESPAWN = "$despawn".freeze    # objects that are gone: their keys
  RESEND = "$resend".freeze      # to the host: send the spawned objects again (after loading a level)
  # Who answers each of them (see the methods of those names).
  ENGINE_MESSAGES = {
    SYNC => :receive_sync, SPAWN => :receive_spawn, SPAWNED => :receive_spawned,
    DESPAWN => :receive_despawn, RESEND => :receive_resend
  }.freeze
  # What the engine does about what happens on the network, before the game hears of it.
  REACTIONS = {
    connected: :enter_game, joined: :greet, left: :remove_objects_of, disconnected: :keep_only_my_objects
  }.freeze
  # ... and after it (the game's handlers may still ask who was there).
  AFTERWARDS = { left: :forget_player, disconnected: :forget_players }.freeze

  @handlers = {}
  @listeners = {}

  class << self
    # Where machines meet over the internet: "relay.example.com", or "relay.example.com:9000".
    # Anyone can run one with `redgem-relay`.
    attr_accessor :relay

    # The port games look for each other on. Every copy of a game must use the same one; another
    # game may pick its own to keep to itself.
    attr_writer :discovery_port

    def discovery_port = @discovery_port || DISCOVERY_PORT

    # Starts looking for games hosted on this network; Network.games lists them as they answer.
    def search = __search(discovery_port)

    # Starts a game others can join (leaving the one this machine is in). False when it could not.
    # Machines on the same network find it by themselves, under `name` (the game's title unless
    # said), unless visible: false. With room: true the game is a room on the relay instead:
    # on_connect follows, and Network.room is then the code the others join with.
    # waiting_per_address: how many machines behind one address (players sharing a network that
    # reaches the internet through one address) may be joining at the same moment.
    def host(port: DEFAULT_PORT, name: nil, visible: true, room: false, waiting_per_address: WAITING_PER_ADDRESS)
      leave
      return __host_room(*relay_address) if room

      __host(port.to_i, name.to_s, visible, discovery_port, waiting_per_address.to_i)
    end

    # Starts joining a game (leaving the one this machine is in): at an address, or with
    # room: "K7QM3P" on the relay (or, with an address too, on the dedicated server there).
    # auth: (a hash) goes with it, for the game joined to decide on (Network.authenticate).
    # on_connect or on_fail tells how it went, a moment later.
    def join(address = nil, port: DEFAULT_PORT, room: nil, auth: nil)
      leave
      return join_room(address, port, room, auth) if room
      raise ArgumentError, "join needs an address or a room" if address.nil?

      __join(address.to_s, port.to_i, auth)
    end

    def leave
      was_online = online?
      __leave
      __event(:disconnected, 0, "", nil) if was_online
    end

    # Sends a message. False when it could not be sent (offline, or nobody with that id).
    def emit(name, data = nil, to: :all)
      name = name.to_s
      raise ArgumentError, "message names starting with #{RESERVED} are the engine's" if name.start_with?(RESERVED)

      __deliver(name, data, to)
    end

    # Runs the block with a message's data and the id of who sent it.
    def on(name, &block) = (@handlers[name.to_s] ||= []) << block

    def on_connect(&block) = listen(:connected, block)        # this machine is in the game
    def on_fail(&block) = listen(:failed, block)              # |reason| the join did not work out (reason may be nil)
    def on_disconnect(&block) = listen(:disconnected, block)  # the game is gone
    def on_join(&block) = listen(:joined, block)              # |peer| someone arrived
    def on_leave(&block) = listen(:left, block)               # |peer| someone left

    # Called by the engine for everything that happens on the network.
    def __event(kind, peer, name, data, ticket = 0)
      return receive(name, data, peer) if kind == :message
      return consider(peer, data, ticket) if kind == :approval

      happened(kind, peer, name)
    end

    # Called by the engine when a level is loaded: its objects and handlers are gone. The others
    # are told that this machine's spawned objects went with it, and the host is asked for the
    # objects this machine should see again.
    def __scene_reset
      announce_scene_reset if online?
      @handlers = {}
      @listeners = {}
      @synced = {}
      @spawned = {}
    end

    # Sends any message, the engine's own included (emit is the one for games).
    def __deliver(name, data, to)
      __emit(name, data, { all: EVERYONE, host: HOST }.fetch(to) { to.to_i })
    end

    private

    def listen(kind, block) = (@listeners[kind] ||= []) << block

    # Someone came, went, or the game started or ended: the engine reacts, then the game's handlers
    # hear of it (a failed join's with its reason, when there is one).
    def happened(kind, peer, name)
      __send__(REACTIONS[kind], peer) if REACTIONS[kind]
      told = told_about(kind, peer, name)
      (@listeners[kind] || []).dup.each { |block| block.call(told) }
      __send__(AFTERWARDS[kind], peer) if AFTERWARDS[kind]
    end

    # What the handlers of an event get: the peer, or why a join failed (nil when nobody said).
    def told_about(kind, peer, name)
      return peer unless kind == :failed

      name.to_s.empty? ? nil : name
    end

    # A room by its code: on the dedicated server at `address` when there is one, on the relay otherwise.
    def join_room(address, port, room, auth)
      server = address.nil? ? relay_address : [address.to_s, port.to_i]
      __join_room(*server, room.to_s.strip.upcase, auth)
    end

    # The relay as [address, port].
    def relay_address
      raise ArgumentError, "say where the relay is first: Network.relay = \"relay.example.com\"" if relay.to_s.empty?

      address, port = relay.to_s.split(":")
      [address, (port || DEFAULT_RELAY_PORT).to_i]
    end

    # A message came: one of the engine's own, or one for the game's handlers.
    def receive(name, data, from)
      engine_message = ENGINE_MESSAGES[name]
      return __send__(engine_message, data, from) if engine_message

      (@handlers[name] || []).dup.each { |block| block.call(data, from) }
    end
  end
end
