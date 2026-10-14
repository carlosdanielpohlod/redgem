# What other machines ask this one to spawn and to remove, and whether they may.
module Network
  class << self
    private

    def receive_spawn(record, from)
      place(record) if spawn_allowed?(record, from)
    end

    # Only the host knows every object spawned so far.
    def receive_spawned(records, from)
      records.each { |record| receive_spawn(record, from) } if host_listing?(records, from)
    end

    def host_listing?(records, from) = from == HOST && records.is_a?(Array)

    def receive_despawn(keys, from)
      remove_objects(keys, from) if keys.is_a?(Array)
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
      return false if key_taken?(key)

      [owner, from].any? { |maker| Said.key_of?(key, template, maker) }
    end

    # Whether something here goes by that key already, spawned or not.
    def key_taken?(key) = @spawned.key?(key) || @synced.key?(key)

    def room_for?(owner) = @spawned.size < MAX_SPAWNS && spawned_by(owner).size < MAX_SPAWNS_PER_PEER

    def spawnable?(template)
      source = Scene.find(template)
      return false unless source

      @spawnable.include?(template) || source.props[:prefab]
    end
  end
end
