# Connection approval: on the machine hosting (a dedicated server, mostly), the game decides who
# comes in, from what each newcomer's join carries.
#
#   Network.authenticate do |peer, auth, decide|
#     HTTP.get(USER_URL, headers: { "Authorization" => "Bearer #{auth[:token]}" }) do |response|
#       next decide.deny("not logged in") unless response.ok?
#
#       decide.accept(identity: response.json[:id], name: response.json[:name])
#     end
#   end
#   Network.identity(peer)      # => "user-4821": who `peer` is, once let in (nil when nobody said)
#   Network.player_name(peer)   # => "Ana", when the decision gave a name
#
# Until the game decides, the newcomer waits: on_join comes once it is let in, and one not decided
# on within 10 seconds is turned away. The machine turned away hears why in on_fail. Without a
# handler everyone is let in. The handler lasts until another replaces it, or `authenticate` with
# no block turns it off, across levels too. In a room on a relay, the game of whoever opened the
# room decides: the relay passes the joins on and the decisions back (a relay older than 0.3.0
# lets everyone in).
module Network
  # The decision on one newcomer: accept or deny, once (what comes after does nothing).
  class Decision
    def initialize(peer, ticket)
      @peer = peer
      @ticket = ticket
    end

    # Lets the newcomer in, as `identity` (who it is for the game, from the service that checked
    # it). True when it was let in; false when it was decided on before, or is gone.
    def accept(identity:, name: nil)
      raise ArgumentError, "accept needs the player's identity" if identity.to_s.empty?

      settle(true, "") { Network.__identify(@peer, identity.to_s, name&.to_s) }
    end

    # Turns the newcomer away; it hears `reason` in on_fail.
    def deny(reason = "not let in") = settle(false, reason.to_s)

    private

    # The one decision there is; the block runs once the newcomer is let in.
    def settle(accepted, reason)
      return false if @decided

      @decided = true
      settled = Network.__decide(@ticket, accepted, reason)
      yield if settled && block_given?
      settled
    end
  end

  @identities = {}   # peer => who it is, as the game's decision said
  @names = {}

  class << self
    # The game decides who comes in: the block gets the newcomer's id, what its join carried (a
    # hash, empty when nothing) and the Decision to make.
    def authenticate(&block)
      @authenticator = block
      __approval(!block.nil?)
    end

    def identity(peer) = @identities[peer]
    def player_name(peer) = @names[peer]

    # Called by a Decision that let a newcomer in.
    def __identify(peer, identity, name)
      @identities[peer] = identity
      @names[peer] = name
    end

    private

    def consider(peer, auth, ticket)
      decision = Decision.new(peer, ticket)
      return decision.deny("nobody decides who comes in") unless @authenticator

      @authenticator.call(peer, auth, decision)
    rescue StandardError
      decision.deny("the game could not decide")
      raise
    end

    def forget_player(peer)
      @identities.delete(peer)
      @names.delete(peer)
    end

    def forget_players(_peer)
      @identities = {}
      @names = {}
    end
  end
end
