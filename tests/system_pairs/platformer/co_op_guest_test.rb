# Two machines play the platformer together; this is the one that joins (see co_op_host_test.rb,
# which runs at the same time). It finds the game by looking around the network: no address typed.
PORT_NOTE = "game_port".freeze            # where the host test leaves the port its game is on
DISCOVERY_NOTE = "discovery_port".freeze  # the run's own (see run_pair.sh): no other game answers on it
SETTLE_FRAMES = 30
JOIN_FRAMES = 1800        # the other game may still be starting
GAME_NAME = "platformer".freeze   # the game's title, whatever its capitals
STEP_ASIDE_FRAMES = 50
MIN_HOST_WALK = 1.0       # meters the host's hero walks to its coin

SystemTest.define do |t|
  Network.discovery_port = TestInput.note(DISCOVERY_NOTE).to_i
  t.wait(SETTLE_FRAMES)
  manager = GameManager.instance
  co_op = CoOp.instance
  port = 0
  t.wait_until(frames: JOIN_FRAMES, message: "the host never said where its game is") do
    port = TestInput.note(PORT_NOTE).to_i
    port.positive?
  end
  Network.search
  found = nil
  t.wait_until(frames: JOIN_FRAMES, message: "the host's game was not found on the network") do
    found = Network.games.find { |game| game[:port] == port }
  end
  t.assert("the game is found under its title (#{found[:name]}), with its host in it") do
    found[:name].downcase == GAME_NAME && found[:players] == 1
  end
  t.assert("the join starts") { Network.join(found[:address], port: found[:port]) }
  t.wait_until(frames: JOIN_FRAMES, message: "could not join the host") { Network.online? }
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
  # The host is told, and its answer waited for: the game is not closed on a message still on its way.
  heard_farewell = false
  Network.on(:farewell) { heard_farewell = true }
  Network.emit(:seen_it_all)
  t.wait_until(frames: JOIN_FRAMES, message: "the host never said farewell") { heard_farewell }
end
