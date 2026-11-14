# A client asks the host for its hero to use something (see Interaction): the host finds both by
# the names the machines know them by, and Interaction looks at the use again before it happens.
module Network
  INTERACTION_SIZE = 2   # parts of what asks for a use: the actor's key, the target's

  class << self
    # What an object is called on the network: its NetworkSync's key, or its name.
    def __key_of(object) = object.get_component(NetworkSync)&.key || object.name

    private

    def receive_interact(data, from)
      return unless host? && interaction?(data)

      actor = actor_of(data[0], from)
      target = object_called(data[1])
      Interaction.__asked(actor, target) if actor && target
    end

    # Two keys, as a client sends them.
    def interaction?(data) = Said.list?(data, INTERACTION_SIZE) && data.all? { |key| network_key?(key) }

    def network_key?(key) = key.is_a?(String) && !key.empty? && key.bytesize <= Said::MAX_KEY_SIZE

    # Only the machine that owns a hero can say what it uses.
    def actor_of(key, from)
      sync = @synced[key]
      sync.game_object if owned_by?(sync, from)
    end

    def owned_by?(sync, peer) = !sync.nil? && sync.owner == peer

    def object_called(key) = @synced[key]&.game_object || Scene.find(key)
  end
end
