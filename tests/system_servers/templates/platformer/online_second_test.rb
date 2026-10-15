# Two players meet on a dedicated server running the platformer template (see
# run_with_server.sh); this one comes second, into the first one's room, and walks for it to see
# (see online_first_test.rb).
SERVER_HOST = "127.0.0.1".freeze
SERVER_NOTE = "server_port".freeze   # where the runner leaves the server's port
ROOM_NOTE = "room".freeze            # where the first test leaves the code of its room
SETTLE_FRAMES = 30
JOIN_FRAMES = 1800        # the other game may still be starting
WALK_FRAMES = 90
CODE_LETTERS = 6

SystemTest.define do |t|
  t.wait(SETTLE_FRAMES)
  online = Camera.main.get_component(Online)
  room = nil
  t.wait_until(frames: JOIN_FRAMES, message: "the first player never got in") do
    room = TestInput.note(ROOM_NOTE).to_s
    room.size == CODE_LETTERS
  end
  online.server = "#{SERVER_HOST}:#{TestInput.note(SERVER_NOTE)}"
  online.play
  t.wait_until(frames: JOIN_FRAMES, message: "could not join the server") { Network.online? }
  t.assert("quick play put us in the first player's room") { Network.room == room && Network.players.size == 1 }
  t.wait_until(frames: SETTLE_FRAMES, message: "the first player's hero did not appear") { online.others.size == 1 }
  t.assert("the one hero we see is the first player's") { online.others.keys == Network.players }
  t.hold(:d, frames: WALK_FRAMES)
  t.screenshot("template_online_second")

  heard_farewell = false
  Network.on(:farewell) { heard_farewell = true }
  Network.emit(:seen_it_all)
  t.wait_until(frames: JOIN_FRAMES, message: "the first player never said farewell") { heard_farewell }
end
