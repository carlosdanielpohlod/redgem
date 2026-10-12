# Two machines play the platformer in a room on a relay (which the test runner starts, and whose
# port it leaves in a note); this one opens the room and leaves its code where the other test
# finds it (see co_op_room_guest_test.rb).
RELAY_HOST = "127.0.0.1".freeze
RELAY_NOTE = "relay_port".freeze   # where the test runner leaves the relay's port (see run_pair.sh)
CODE_NOTE = "room_code".freeze     # where the host test leaves the code for the guest test
SETTLE_FRAMES = 30
JOIN_FRAMES = 1800        # the other game may still be starting
CODE_LETTERS = 6

SystemTest.define do |t|
  t.wait(SETTLE_FRAMES)
  co_op = CoOp.instance
  Network.relay = "#{RELAY_HOST}:#{TestInput.note(RELAY_NOTE)}"
  t.assert("the relay is there to call") { Network.host(room: true) }
  t.wait_until(frames: JOIN_FRAMES, message: "could not open a room on the relay") { Network.online? }
  t.assert("the room has a code, and whoever opened it hosts the game") do
    Network.room.size == CODE_LETTERS && Network.host? && Network.id == 1
  end
  TestInput.leave_note(CODE_NOTE, Network.room)
  guest_done = false
  Network.on(:seen_it_all) do
    guest_done = true
    Network.emit(:farewell)
  end

  t.wait_until(frames: JOIN_FRAMES, message: "nobody came into the room") { co_op.ghosts.size == 1 }
  t.hold(:d, frames: SETTLE_FRAMES)
  t.wait_until(frames: JOIN_FRAMES, message: "the guest never saw us move") { guest_done }
  # The guest goes once it heard the farewell, and the room closes after it: nobody's last word is cut off.
  t.wait_until(frames: JOIN_FRAMES, message: "the guest never left") { Network.peers.empty? }
end
