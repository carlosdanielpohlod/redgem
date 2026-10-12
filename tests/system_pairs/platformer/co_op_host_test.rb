# Two machines play the platformer together; this is the one that hosts (see co_op_guest_test.rb,
# which runs at the same time). Each sees the other's hero move, and a coin picked up here is
# gone there. The two tests tell each other what the other cannot know in notes: where the game
# is hosted, on a port nobody else has.
ANY_PORT = 0              # whichever is free: the guest reads which one it was
PORT_NOTE = "game_port".freeze            # where this test leaves the port for the guest test
DISCOVERY_NOTE = "discovery_port".freeze  # the run's own (see run_pair.sh): no other game answers on it
SETTLE_FRAMES = 30
JOIN_FRAMES = 1800        # the other game may still be starting
WALK_FRAMES = 60
MIN_GHOST_MOVE = 1.0      # meters the guest's hero walks aside

SystemTest.define do |t|
  Network.discovery_port = TestInput.note(DISCOVERY_NOTE).to_i
  t.wait(SETTLE_FRAMES)
  manager = GameManager.instance
  co_op = CoOp.instance
  t.assert("the game can be hosted") { Network.host(port: ANY_PORT) }
  TestInput.leave_note(PORT_NOTE, Network.port.to_s)
  guest_done = false
  Network.on(:seen_it_all) do
    guest_done = true
    Network.emit(:farewell)
  end

  t.wait_until(frames: JOIN_FRAMES, message: "nobody joined") { co_op.ghosts.size == 1 }
  ghost = co_op.ghosts.values.first
  t.assert("the guest's hero is here, and is not ours to move") { ghost.alive? && ghost.get_component(Player).ghost? }
  t.assert("our own hero still is") { !Scene.find("Player").get_component(Player).ghost? }
  ghost_start = ghost.transform.position
  t.wait_until(frames: JOIN_FRAMES, message: "the guest's hero never moved here") do
    ghost.transform.position.distance_to(ghost_start) > MIN_GHOST_MOVE
  end

  t.hold(:w, frames: WALK_FRAMES)
  t.wait_until(frames: SETTLE_FRAMES, message: "Coin1 was not collected") { Scene.find("Coin1").nil? }
  t.assert("the coin counts here") { manager.coins == 1 }
  t.screenshot("co_op_host")
  t.wait_until(frames: JOIN_FRAMES, message: "the guest never saw the coin go") { guest_done }
  # The guest goes once it heard the farewell, and the game closes after it: nobody's last word is cut off.
  t.wait_until(frames: JOIN_FRAMES, message: "the guest never left") { Network.peers.empty? }
end
