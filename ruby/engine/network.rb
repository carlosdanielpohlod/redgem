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
    connected: :enter_game, joined: :send_spawned_to, left: :remove_objects_of, disconnected: :keep_only_my_objects
  }.freeze

  @handlers = {}
  @listeners = {}
  @synced = {}
  @spawned = {}
  @spawnable = []
  @spawn_count = 0

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
    def host(port: DEFAULT_PORT, name: nil, visible: true, room: false)
      leave
      return __host_room(*relay_address) if room

      __host(port.to_i, name.to_s, visible, discovery_port)
    end

    # Starts joining a game (leaving the one this machine is in): at an address, or with
    # room: "K7QM3P" on the relay. on_connect or on_fail tells how it went, a moment later.
    def join(address = nil, port: DEFAULT_PORT, room: nil)
      leave
      return __join_room(*relay_address, room.to_s.strip.upcase) if room
      raise ArgumentError, "join needs an address or a room" if address.nil?

      __join(address.to_s, port.to_i)
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
    def on_fail(&block) = listen(:failed, block)              # the join did not work out
    def on_disconnect(&block) = listen(:disconnected, block)  # the game is gone
    def on_join(&block) = listen(:joined, block)              # |peer| someone arrived
    def on_leave(&block) = listen(:left, block)               # |peer| someone left

    # Copies a template (see Scene.spawn) on every machine in the game, now and for whoever joins
    # later. The copy belongs to `owner`: that machine moves it, with its physics, and the others
    # watch it move (it gets a NetworkSync, and no physics there). It goes away with its owner.
    def spawn(template, owner: id, position: nil, rotation: nil)
      allow_spawn(template)
      record = [next_key(template), template.to_s, owner, position&.to_a, rotation&.to_a]
      __deliver(SPAWN, record, :all)
      place(record)
    end

    # Other machines may only make this one spawn templates it spawns itself, `prefab = true`
    # objects, and the ones named here.
    def allow_spawn(*templates)
      templates.each { |template| @spawnable << template.to_s unless @spawnable.include?(template.to_s) }
    end

    def __track(sync) = @synced[sync.key] = sync

    def __untrack(sync)
      @synced.delete(sync.key) if @synced[sync.key].equal?(sync)
    end

    # The object is gone for good.
    def __forget(sync)
      __untrack(sync)
      @spawned.delete(sync.key)
    end

    # Called by the engine for everything that happens on the network.
    def __event(kind, peer, name, data)
      return receive(name, data, peer) if kind == :message

      __send__(REACTIONS[kind], peer) if REACTIONS[kind]
      (@listeners[kind] || []).dup.each { |block| block.call(peer) }
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

    # The relay as [address, port].
    def relay_address
      raise ArgumentError, "say where the relay is first: Network.relay = \"relay.example.com\"" if relay.to_s.empty?

      address, port = relay.to_s.split(":")
      [address, (port || DEFAULT_RELAY_PORT).to_i]
    end

    def announce_scene_reset
      mine = keys_spawned_by(id)
      __deliver(DESPAWN, mine, :all) unless mine.empty?
      __deliver(RESEND, nil, :host) unless host?
    end

    def enter_game(_peer)
      @my_id = id
      adopt_local_objects
    end

    def next_key(template)
      @spawn_count += 1
      "#{template}##{id}.#{@spawn_count}"
    end

    # A message came: one of the engine's own, or one for the game's handlers.
    def receive(name, data, from)
      engine_message = ENGINE_MESSAGES[name]
      return __send__(engine_message, data, from) if engine_message

      (@handlers[name] || []).dup.each { |block| block.call(data, from) }
    end

    def receive_sync(data, from)
      @synced[data[0]]&.__receive(data[1], data[2], from) if Said.placement?(data)
    end

    def receive_spawn(record, from)
      place(record) if spawn_allowed?(record, from)
    end

    # Only the host knows every object spawned so far.
    def receive_spawned(records, from)
      records.each { |record| receive_spawn(record, from) } if from == HOST && records.is_a?(Array)
    end

    def receive_despawn(keys, from)
      remove_objects(keys, from) if keys.is_a?(Array)
    end

    def receive_resend(_nothing, from) = send_spawned_to(from)

    # The host tells a machine (a newcomer, or one that asked again) what is spawned.
    def send_spawned_to(peer)
      __deliver(SPAWNED, spawned_records, peer) if host?
    end

    # Whether an object another machine asks for may be created here: only for itself (the host
    # speaks for everyone in the game), under a key that is its maker's to give and that nothing
    # here goes by yet, from a template this level has and this game spawns, and not too many.
    def spawn_allowed?(record, from)
      return false unless Said.record?(record)

      key, template, owner = record
      speaks_for?(from, owner) && free_key?(key, template, owner, from) && spawnable?(template) && room_for?(owner)
    end

    def speaks_for?(peer, owner) = peer == owner || (peer == HOST && in_game?(owner))

    def in_game?(peer) = peer == HOST || peer == id || peers.include?(peer)

    # The key's maker is the object's owner, or the host when it spawned the object for the owner.
    def free_key?(key, template, owner, from)
      return false if @spawned.key?(key) || @synced.key?(key)

      [owner, from].any? { |maker| Said.key_of?(key, template, maker) }
    end

    def room_for?(owner) = @spawned.size < MAX_SPAWNS && spawned_by(owner).size < MAX_SPAWNS_PER_PEER

    def spawnable?(template)
      source = Scene.find(template)
      return false unless source

      @spawnable.include?(template) || source.props[:prefab]
    end

    # What `owner` has spawned: [key, entry] pairs.
    def spawned_by(owner) = @spawned.to_a.select { |_key, entry| entry[:owner] == owner }

    def keys_spawned_by(owner) = spawned_by(owner).map(&:first)

    def place(record)
      key, template, owner, position, rotation = record
      return @spawned[key][:object] if @spawned[key]

      mine = !online? || owner == id
      object = Scene.spawn(template, position: position && Vector3[*position],
                                     rotation: rotation && Vector3[*rotation], physics: mine)
      object.add_component(NetworkSync, owner: owner, key: key)
      @spawned[key] = { template: template, owner: owner, object: object }
      object
    end

    def record_of(key, entry)
      transform = entry[:object].transform
      [key, entry[:template], entry[:owner], transform.position.to_a, transform.rotation.to_a]
    end

    # What a newcomer needs to see the objects spawned before it arrived, where they are now.
    def spawned_records
      @spawned.to_a.select { |_key, entry| entry[:object].alive? }.map { |key, entry| record_of(key, entry) }
    end

    # This machine just got into a game: what it spawned while in none becomes its own there, under
    # names nobody else uses, and the others are told.
    def adopt_local_objects
      spawned_by(LOCAL).each { |key, entry| adopt(key, entry) if entry[:object].alive? }
    end

    def adopt(local_key, entry)
      @spawned.delete(local_key)
      entry[:owner] = id
      key = next_key(entry[:template])
      @spawned[key] = entry
      entry[:object].get_component(NetworkSync).assign(owner: id, key: key)
      __deliver(SPAWN, record_of(key, entry), :all)
    end

    def remove_objects_of(peer)
      keys_spawned_by(peer).each { |key| remove(key) }
    end

    def remove_objects(keys, owner)
      keys.each { |key| remove(key) if @spawned[key] && @spawned[key][:owner] == owner }
    end

    def remove(key)
      @spawned.delete(key)[:object].destroy
    end

    # The game is gone: what this machine was moving stays, as its own; the rest leaves with the
    # others.
    def keep_only_my_objects(_peer)
      @spawned.to_a.each do |key, entry|
        sync = entry[:object].get_component(NetworkSync)
        next remove(key) unless sync.owner == @my_id

        entry[:owner] = LOCAL
        sync.owner = LOCAL
      end
    end
  end
end

module Network
  # What other machines say about objects, checked before any of it is used: whoever sent it may
  # not be running this game's code. What does not fit is ignored.
  module Said
    MAX_PEERS = 16            # machines in one game, the host included
    RECORD_SIZE = 5           # parts of a spawn record: key, template, owner, position, rotation
    SYNC_SIZE = 3             # parts of what says where an object is: key, position, rotation
    VECTOR_SIZE = 3           # numbers a position or a rotation travels as: x, y, z
    MAX_KEY_SIZE = 128        # letters in the key of a spawned object
    MAX_COUNT_DIGITS = 9      # in the number such a key ends with
    MAX_DISTANCE = 1_000_000.0   # meters from the middle of a level (or degrees of a turn) an object can be at

    class << self
      # A spawn record with every part of its kind.
      def record?(record) = record.is_a?(Array) && record.size == RECORD_SIZE && right_parts?(*record)

      def right_parts?(key, template, owner, position, rotation)
        [key, template].all?(String) && peer_id?(owner) && [position, rotation].compact.all? { |part| vector?(part) }
      end

      def peer_id?(owner) = owner.is_a?(Integer) && owner.between?(HOST, MAX_PEERS)

      # Three numbers an object can be at, or be turned by.
      def vector?(part) = part.is_a?(Array) && part.size == VECTOR_SIZE && part.all? { |number| reachable?(number) }

      # (Infinity and what is not a number compare as false, so they are out too.)
      def reachable?(number) = number.is_a?(Numeric) && number.abs <= MAX_DISTANCE

      # Where an object is, as its owner sends it: [key, position, rotation].
      def placement?(data)
        return false unless data.is_a?(Array) && data.size == SYNC_SIZE

        data.first.is_a?(String) && data.drop(1).all? { |part| vector?(part) }
      end

      # Whether `key` is one `maker` gives what it spawns from `template`: "Ball#2.7" is the
      # seventh thing machine 2 spawned. Nothing else is a spawned object's key, so no machine
      # can take a key another one's objects go by, now or later.
      def key_of?(key, template, maker)
        start = "#{template}##{maker}."
        key.size <= MAX_KEY_SIZE && key.start_with?(start) && count?(key[start.size, key.size])
      end

      # A count as it is written into a key: digits, with no sign and no zero in front.
      def count?(text) = text.size.between?(1, MAX_COUNT_DIGITS) && text.to_i.positive? && text.to_i.to_s == text
    end
  end
end

# Keeps an object in the same place on every machine in the game: the machine that owns it moves
# it as usual, the others see it follow. Both sides need the object, with the same key.
# Settings: owner (a peer's id; the host by default, right for the level's own moving things),
# key (what the object is called on the network; its name by default).
#
#   player.add_component(NetworkSync, owner: Network.id, key: "hero-#{Network.id}")
#   return unless get_component(NetworkSync).mine?   # only the owner reads the keyboard
class NetworkSync < Behaviour
  SENDS_PER_SECOND = 20.0
  SMOOTHING = 14.0        # how fast a watched object closes in on where it should be (per second)
  SNAP_DISTANCE = 6.0     # meters: further behind than this, it jumps there
  FULL_TURN = 360.0
  HALF_TURN = 180.0

  attr_writer :owner

  def owner = @owner ||= prop(:owner, Network::HOST).to_i
  def key = @key ||= prop(:key, name).to_s
  # Offline, everything is this machine's to move.
  def mine? = !Network.online? || owner == Network.id

  def start
    Network.__track(self)
    @since_sent = 0.0
  end

  # Hands the object to another owner, under another name on the network if need be. Only on this
  # machine: the others need the same owner and name for it, by a message of the game's.
  def assign(owner:, key: self.key)
    Network.__untrack(self)
    @owner = owner
    @key = key.to_s
    Network.__track(self)
  end

  def on_destroy = Network.__forget(self)

  # An angle part of the way to another, the short way round.
  def self.toward(from, to, share)
    from + ((to - from + HALF_TURN) % FULL_TURN - HALF_TURN) * share
  end

  def update(dt)
    return follow(dt) unless mine?
    return unless Network.online?

    @since_sent += dt
    return if @since_sent < 1.0 / SENDS_PER_SECOND

    @since_sent = 0.0
    Network.__deliver(Network::SYNC, [key, transform.position.to_a, transform.rotation.to_a], :all)
  end

  # Where the owner says the object is. Only the owner is believed.
  def __receive(position, rotation, from)
    return unless from == owner

    @position = Vector3[*position]
    @rotation = Vector3[*rotation]
  end

  private

  def follow(dt)
    return unless @position

    closer = [SMOOTHING * dt, 1.0].min
    transform.position = position_closer(closer)
    transform.rotation = rotation_closer(closer)
  end

  # Part of the way to where the owner says it is; all the way when it fell too far behind.
  def position_closer(share)
    here = transform.position
    here.distance_to(@position) > SNAP_DISTANCE ? @position : here.lerp(@position, share)
  end

  def rotation_closer(share)
    angles = transform.rotation.to_a.zip(@rotation.to_a)
    Vector3[*angles.map { |from, to| NetworkSync.toward(from, to, share) }]
  end
end
