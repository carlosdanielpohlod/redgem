# Two machines play the platformer together; this is the one that joins (see co_op_host_test.rb,
# which runs at the same time).
PORT = 47_821
HOST_ADDRESS = "127.0.0.1"
SETTLE_FRAMES = 30
JOIN_FRAMES = 1800        # the other game may still be starting
RETRY_FRAMES = 20         # between attempts to join
STEP_ASIDE_FRAMES = 50
MIN_HOST_WALK = 1.0       # meters the host's hero walks to its coin
PARTING_FRAMES = 30       # for the last message to leave before the game closes

SystemTest.define do |t|
  t.wait(SETTLE_FRAMES)
  manager = GameManager.instance
  co_op = CoOp.instance
  t.wait_until(frames: JOIN_FRAMES, message: "could not join the host") do
    Network.join(HOST_ADDRESS, port: PORT) unless Network.online? || Network.connecting?
    t.wait(RETRY_FRAMES) unless Network.online?
    Network.online?
  end
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
