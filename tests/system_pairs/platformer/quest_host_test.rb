# A guest's hero finishes the second stage of the old tree's quest; this is the host (see
# quest_guest_test.rb, which runs at the same time). The quest is the host's: it moves it to that
# stage, counts the coin and the finish line the guest says its hero reached, and completes it.
ANY_PORT = 0              # whichever is free: the guest reads which one it was
PORT_NOTE = "game_port".freeze   # where this test leaves the port for the guest test
SETTLE_FRAMES = 30
JOIN_FRAMES = 1800        # the other game may still be starting

SystemTest.define do |t|
  t.wait(SETTLE_FRAMES)
  co_op = CoOp.instance
  completed = []
  Quests.on(:complete) { |quest| completed << quest.name }
  t.assert("the game can be hosted") { Network.host(port: ANY_PORT, visible: false) }
  TestInput.leave_note(PORT_NOTE, Network.port.to_s)
  guest_done = false
  Network.on(:quest_seen) { guest_done = true }

  t.wait_until(frames: JOIN_FRAMES, message: "nobody joined") { co_op.ghosts.size == 1 }
  t.assert("the host moves the quest") { Quests.set_stage(:tree, "goal") == "goal" }
  quest = Quests[:tree]
  t.wait_until(frames: JOIN_FRAMES, message: "the guest's coin never counted here") { quest.objectives.first&.done? }
  t.wait_until(frames: JOIN_FRAMES, message: "the guest never reached the finish") { quest.complete? }
  t.assert("the host completed it once") { completed == ["tree"] }
  t.wait_until(frames: JOIN_FRAMES, message: "the guest never saw it complete") { guest_done }
  Network.emit(:farewell)
  # The guest goes once it heard the farewell, and the game closes after it.
  t.wait_until(frames: JOIN_FRAMES, message: "the guest never left") { Network.peers.empty? }
end
