# Two machines play the platformer together; this is the one that joins (see co_op_host_test.rb,
# which runs at the same time). It finds the game by looking around the network: no address typed.
PORT = 47_821
SETTLE_FRAMES = 30
JOIN_FRAMES = 1800        # the other game may still be starting
RETRY_FRAMES = 20         # between attempts to join
GAME_NAME = "platformer".freeze   # the game's title, whatever its capitals
STEP_ASIDE_FRAMES = 50
MIN_HOST_WALK = 1.0       # meters the host's hero walks to its coin
PARTING_FRAMES = 30       # for the last message to leave before the game closes

DISCOVERY_PORT = 47_833   # the test's own: a game open on this machine does not answer on it

SystemTest.define do |t|
  Network.discovery_port = DISCOVERY_PORT
  t.wait(SETTLE_FRAMES)
  manager = GameManager.instance
  co_op = CoOp.instance
  Network.search
  found = nil
  t.wait_until(frames: JOIN_FRAMES, message: "the host's game was not found on the network") do
    found = Network.games.find { |game| game[:port] == PORT }
  end
  t.assert("the game is found under its title (#{found[:name]}), with its host in it") do
    found[:name].downcase == GAME_NAME && found[:players] == 1
  end
  t.wait_until(frames: JOIN_FRAMES, message: "could not join the host") do
    Network.join(found[:address], port: found[:port]) unless Network.online? || Network.connecting?
    t.wait(RETRY_FRAMES) unless Network.online?
    Network.online?
  end
  Network.stop_search
  t.assert("we are the second machine in the game") { Network.id == 2 && Network.peers == [1] }
  t.wait_until(frames: SETTLE_FRAMES, message: "the host's hero did not appear") { co_op.ghosts.size == 1 }
  ghost = co_op.ghosts.values.first
  ghost_start = ghost.transform.position

  # Out of the way, so the host sees us move and can walk to its coin.
  t.hold(:d, frames: STEP_ASIDE_FRAMES)
  t.wait_until(frames: JOIN_FRAMES, message: "the host's coin never went away here") { Scene.find("Coin1").nil? }
  t.assert("a coin the host picked up counts here too") { manager.coins == 1 && manager.collected == ["Coin1"] }
  t.assert("the host's hero was seen walking") { ghost.transform.position.distance_to(ghost_start) > MIN_HOST_WALK }
  t.assert("the other coins are still here") { Scene.find_all(:coin).size == 4 }
  t.screenshot("co_op_guest")
  Network.emit(:seen_it_all)
  t.wait(PARTING_FRAMES)
end
