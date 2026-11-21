# Starting a game: alone, hosting one that others join, joining one by its address, or joining the
# game's dedicated server (a machine running `redgem-server` on this game). The lobby screen
# (LobbyScreen) and the tests call these. Playing alone or hosting goes on from the save (Progress).
module Session
  # Where your dedicated server runs: "play.example.com", or "play.example.com:7777".
  SERVER = "".freeze

  # Plays alone, from where the save left the party.
  def self.play = Progress.resume

  # Hosts a game (port 0: any free one, see Network.port). visible: false hides it from the
  # games looking on the same network.
  def self.host(port: Network::DEFAULT_PORT, visible: true)
    hosted = Network.host(port: port, name: Game.title, visible: visible)
    Progress.resume if hosted
    hosted
  end

  # Joins a game hosted at that address: the host's level, quest and save are the party's.
  def self.join(address, port: Network::DEFAULT_PORT) = Network.join(address, port: port)

  # Joins the dedicated server ("address" or "address:port"), in any room with a free place.
  def self.play_on_server(server = SERVER)
    return false if server.to_s.empty?

    address, port = server.split(":")
    Network.join(address, port: (port || Network::DEFAULT_PORT).to_i)
  end

  def self.leave = Network.leave
end
