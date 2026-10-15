# On the dedicated server: who may play (see Accounts), and what is kept for each player. A token
# lets a player in once the backend says whose it is; with no backend, everyone comes in, as a
# guest (a machine that carries no guest id, such as one of an older version, plays anonymously
# and nothing is kept for it). The coins
# each player ever picked up are kept under the player's identity in the server's Save (its data
# folder) and, with a backend, there too: the backend takes them only from the server, whose key
# is in the server's own Save under "backend_key" (put there by whoever runs it; players never
# see a server's saves).
class ServerGate
  PLAYERS_KEY = "players".freeze
  BACKEND_KEY = "backend_key".freeze
  COINS = "coins".freeze
  NAME = "name".freeze
  GUEST = "guest-".freeze
  USER = "user-".freeze
  ANONYMOUS = "anonymous".freeze

  def initialize
    @coins = Hash.new(0)   # picked up in this match, by peer
    Network.authenticate { |_peer, auth, decide| admit(auth, decide) }
    Network.on(CoOp::COIN) { |_name, from| @coins[from] += 1 }
    Network.on_leave { |peer| keep(peer) }
  end

  private

  def admit(auth, decide)
    return check(auth[:token].to_s, decide) if Accounts.backend?

    decide.accept(identity: guest_id?(auth[:guest]) ? "#{GUEST}#{auth[:guest]}" : ANONYMOUS)
  end

  # Asks the backend whose the token is.
  def check(token, decide)
    HTTP.get("#{Accounts.backend}/me", headers: { "Authorization" => "Bearer #{token}" }) do |response|
      next decide.deny("not logged in") unless response.ok?

      decide.accept(identity: "#{USER}#{response.json[:id]}", name: response.json[:name])
    end
  end

  def guest_id?(id) = id.is_a?(String) && id.size == Accounts::GUEST_ID_DIGITS && hex?(id)
  def hex?(text) = text.each_char.all? { |letter| Accounts::HEX.include?(letter) }

  def keep(peer)
    identity = Network.identity(peer)
    return if identity == ANONYMOUS

    coins = @coins.delete(peer) || 0
    players = Save[PLAYERS_KEY] || {}
    players[identity] = record(players[identity], coins, Network.player_name(peer))
    Save[PLAYERS_KEY] = players
    report(identity, coins)
  end

  def record(kept, coins, name) = { COINS => (kept || {}).fetch(COINS, 0) + coins, NAME => name }

  # The backend keeps a logged-in player's coins too, when the server holds its key.
  def report(identity, coins)
    key = Save[BACKEND_KEY]
    return unless key && identity.start_with?(USER)

    HTTP.post("#{Accounts.backend}/progress", json: { player: identity[USER.size..].to_i, coins: coins },
                                              headers: { "X-Server-Key" => key.to_s }) do |response|
      Debug.warn("the backend kept no coins: #{response.error || response.status}") unless response.ok?
    end
  end
end
