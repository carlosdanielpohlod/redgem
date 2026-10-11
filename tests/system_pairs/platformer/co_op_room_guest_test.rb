# Two machines play the platformer in a room on a relay; this one joins with the room's code
# (see co_op_room_host_test.rb, which opens the room and leaves the code in a file).
RELAY = "127.0.0.1:47841".freeze
CODE_NOTE = "room_code"   # where the host test leaves the code for the guest test
SETTLE_FRAMES = 30
JOIN_FRAMES = 1800        # the other game may still be starting
RETRY_FRAMES = 20
CODE_LETTERS = 6
MIN_HOST_WALK = 0.5       # meters the host's hero steps aside
PARTING_FRAMES = 30       # for the last message to leave before the game closes

SystemTest.define do |t|
  t.wait(SETTLE_FRAMES)
  co_op = CoOp.instance
  Network.relay = RELAY
  code = nil
  t.wait_until(frames: JOIN_FRAMES, message: "the host never opened a room") do
    code = TestInput.note(CODE_NOTE)
    code.to_s.size == CODE_LETTERS
  end
  t.wait_until(frames: JOIN_FRAMES, message: "could not join the room") do
    Network.join(room: code) unless Network.online? || Network.connecting?
    t.wait(RETRY_FRAMES) unless Network.online?
    Network.online?
  end
  t.assert("we are in the host's room as the second machine") { Network.room == code && Network.id == 2 }
  t.wait_until(frames: SETTLE_FRAMES, message: "the host's hero did not appear") { co_op.ghosts.size == 1 }
  ghost = co_op.ghosts.values.first
  ghost_start = ghost.transform.position
  t.wait_until(frames: JOIN_FRAMES, message: "the host's hero was never seen moving") do
    ghost.transform.position.distance_to(ghost_start) > MIN_HOST_WALK
  end
  Network.emit(:seen_it_all)
  t.wait(PARTING_FRAMES)
end
