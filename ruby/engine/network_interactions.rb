# A client asks the host for its hero to use something (see Interaction): the host finds both by
# the names the machines know them by, and Interaction looks at the use again before it happens.
module Network
  class << self
    # What an object is called on the network: its NetworkSync's key, or its name.
    def __key_of(object) = object.get_component(NetworkSync)&.key || object.name

    private

    def receive_interact(data, from)
      return unless host? && Said.interaction?(data)

      actor = actor_of(data[0], from)
      Interaction.__asked(actor, objects_called(data[1])) if actor
    end

    # Only the machine that owns a hero can say what it uses.
    def actor_of(key, from)
      sync = @synced[key]
      sync.game_object if owned_by?(sync, from)
    end

    def owned_by?(sync, peer) = !sync.nil? && sync.owner == peer

    # The object that goes by a key on the network, or the Interactables' objects of that name
    # (several objects may share one: the use goes to the one in reach).
    def objects_called(key)
      synced = @synced[key]&.game_object
      return [synced] if synced

      Interactable.all.map(&:game_object).select { |object| object.name == key }
    end
  end
end
