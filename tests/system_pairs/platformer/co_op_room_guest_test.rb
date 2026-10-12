# Two machines play the platformer in a room on a relay; this one joins with the room's code
# (see co_op_room_host_test.rb, which opens the room and leaves the code in a note).
RELAY_HOST = "127.0.0.1".freeze
RELAY_NOTE = "relay_port".freeze   # where the test runner leaves the relay's port (see run_pair.sh)
CODE_NOTE = "room_code".freeze     # where the host test leaves the code for the guest test
SETTLE_FRAMES = 30
JOIN_FRAMES = 1800        # the other game may still be starting
CODE_LETTERS = 6
MIN_HOST_WALK = 0.5       # meters the host's hero steps aside

SystemTest.define do |t|
  t.wait(SETTLE_FRAMES)
  co_op = CoOp.instance
  Network.relay = "#{RELAY_HOST}:#{TestInput.note(RELAY_NOTE)}"
  code = nil
  t.wait_until(frames: JOIN_FRAMES, message: "the host never opened a room") do
    code = TestInput.note(CODE_NOTE)
    code.to_s.size == CODE_LETTERS
  end
  t.assert("the relay is there to call") { Network.join(room: code) }
  t.wait_until(frames: JOIN_FRAMES, message: "could not join the room") { Network.online? }
  t.assert("we are in the host's room as the second machine") { Network.room == code && Network.id == 2 }
  t.wait_until(frames: SETTLE_FRAMES, message: "the host's hero did not appear") { co_op.ghosts.size == 1 }
  ghost = co_op.ghosts.values.first
  ghost_start = ghost.transform.position
  t.wait_until(frames: JOIN_FRAMES, message: "the host's hero was never seen moving") do
    ghost.transform.position.distance_to(ghost_start) > MIN_HOST_WALK
  end
  # The host is told, and its answer waited for: the game is not closed on a message still on its way.
  heard_farewell = false
  Network.on(:farewell) { heard_farewell = true }
  Network.emit(:seen_it_all)
  t.wait_until(frames: JOIN_FRAMES, message: "the host never said farewell") { heard_farewell }
end
