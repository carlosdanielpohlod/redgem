# The objects kept in step on this machine, by the key each goes by on the network.
module Network
  @synced = {}

  class << self
    # The object goes by `key` on the network from now on. A key is one object's: another one of
    # the scene going by it already is a mistake of the game's, refused.
    def __track(sync, key = sync.key)
      refuse_taken(key, sync)
      __untrack(sync)
      @synced[key] = sync
    end

    def __untrack(sync)
      @synced.delete(sync.key) if @synced[sync.key].equal?(sync)
    end

    # Any destroyed object: if it was spawned on the network, it is no longer, whether or not its
    # NetworkSync ever started (only a started one forgets it on its own).
    def __object_gone(object)
      return if @spawned.empty?

      key, entry = @spawned.find { |_key, spawned| spawned[:object] == object }
      forget_spawned(key, entry) if entry
    end

    # The object is gone for good. (Its key may be one a spawned object goes by too: only what was
    # spawned as this very object is forgotten with it.)
    def __forget(sync)
      __untrack(sync)
      entry = @spawned[sync.key]
      forget_spawned(sync.key, entry) if spawned_as?(entry, sync.game_object)
    end

    private

    def refuse_taken(key, sync)
      holder = @synced[key]
      return unless holds_key?(holder, sync)

      raise ArgumentError, "NetworkSync on #{sync.game_object.name.inspect}: the key #{key.inspect} is " \
                           "#{holder.game_object.name.inspect}'s already (give each object its own)"
    end

    # Whether another object, still in the scene, goes by a key.
    def holds_key?(holder, sync) = holder && !holder.equal?(sync) && holder.game_object.alive?

    # Whether what is spawned under a key is that very object (nothing may be spawned under it).
    def spawned_as?(entry, object) = entry && entry[:object] == object

    # Whether an object is this machine's in a game: one the others have to hear about.
    def shared_by_me?(owner) = online? && owner == id

    def receive_sync(data, from)
      @synced[data[0]]&.__receive(data[1], data[2], from) if Said.placement?(data)
    end

    # A spawned object was destroyed here: when it was this machine's, the others lose it too.
    def forget_spawned(key, entry)
      @spawned.delete(key)
      __deliver(DESPAWN, [key], :all) if shared_by_me?(entry[:owner])
    end
  end
end

# Keeps an object in the same place on every machine in the game: the machine that owns it moves
# it as usual, the others see it follow. Both sides need the object, with the same key.
# Settings: owner (a peer's id; the host by default, right for the level's own moving things),
# key (what the object is called on the network; its name by default: one object's only, a second
# NetworkSync under a key taken already raises), interpolate (true by default: the others see it
# a moment behind, moving smoothly, see Snapshots; false: it closes in on the newest pose that
# arrived instead, sooner but jerkier), delay (seconds it is shown behind, always; unset: as short
# as the network allows).
#
#   player.add_component(NetworkSync, owner: Network.id, key: "hero-#{Network.id}")
#   return unless get_component(NetworkSync).mine?   # only the owner reads the keyboard
class NetworkSync < Behaviour
  SENDS_PER_SECOND = 20.0
  SMOOTHING = 14.0        # how fast a watched object closes in on where it should be (per second), not interpolated
  SNAP_DISTANCE = 6.0     # meters: further than this from where it was, it jumps there
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
    @snapshots = Snapshots.new(1.0 / SENDS_PER_SECOND, prop(:delay)&.to_f) if prop(:interpolate, true)
  end

  # Hands the object to another owner, under another name on the network if need be. Only on this
  # machine: the others need the same owner and name for it, by a message of the game's. A name
  # another object goes by is refused, and nothing changes.
  def assign(owner:, key: self.key)
    Network.__track(self, key.to_s)
    @owner = owner
    @key = key.to_s
  end

  def on_destroy = Network.__forget(self)

  # An angle part of the way to another, the short way round.
  def self.toward(from, to, share)
    from + ((to - from + HALF_TURN) % FULL_TURN - HALF_TURN) * share
  end

  def update(dt)
    return watch(dt) unless mine?
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
    @snapshots&.add(@position, @rotation)
  end

  private

  def watch(dt)
    return follow(dt) unless @snapshots

    @snapshots.advance(dt)
    position, rotation = @snapshots.pose
    return unless position

    transform.position = position
    transform.rotation = rotation
  end

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
