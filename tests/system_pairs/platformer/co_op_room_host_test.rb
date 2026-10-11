# Two machines play the platformer in a room on a relay (which the test runner starts); this one
# opens the room and leaves its code where the other test finds it (see co_op_room_guest_test.rb).
RELAY = "127.0.0.1:47841"
CODE_FILE = "/tmp/redgem_test_room_code.txt"
SETTLE_FRAMES = 30
JOIN_FRAMES = 1800        # the other game may still be starting
CODE_LETTERS = 6

SystemTest.define do |t|
  t.wait(SETTLE_FRAMES)
  co_op = CoOp.instance
  Network.relay = RELAY
  t.wait_until(frames: JOIN_FRAMES, message: "could not open a room on the relay") do
    Network.host(room: true) unless Network.online? || Network.connecting?
    t.wait(SETTLE_FRAMES) unless Network.online?
    Network.online?
  end
  t.assert("the room has a code, and whoever opened it hosts the game") do
    Network.room.size == CODE_LETTERS && Network.host? && Network.id == 1
  end
  File.open(CODE_FILE, "w") { |file| file.write(Network.room) }
  guest_done = false
  Network.on(:seen_it_all) { guest_done = true }

  t.wait_until(frames: JOIN_FRAMES, message: "nobody came into the room") { co_op.ghosts.size == 1 }
  t.hold(:d, frames: SETTLE_FRAMES)
  t.wait_until(frames: JOIN_FRAMES, message: "the guest never saw us move") { guest_done }
end
