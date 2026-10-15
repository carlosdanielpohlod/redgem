# Two players meet on a dedicated server running the platformer template (see
# run_with_server.sh); this one arrives first, from the title screen, and watches the other walk
# (see online_second_test.rb).
SERVER_HOST = "127.0.0.1".freeze
SERVER_NOTE = "server_port".freeze   # where the runner leaves the server's port
ROOM_NOTE = "room".freeze            # where this test leaves the code of its room
SETTLE_FRAMES = 30
JOIN_FRAMES = 1800        # the other game may still be starting
CODE_LETTERS = 6
MIN_WALK = 1.0            # meters the other player's hero walks

SystemTest.define do |t|
  t.wait(SETTLE_FRAMES)
  online = Camera.main.get_component(Online)
  online.server = "#{SERVER_HOST}:#{TestInput.note(SERVER_NOTE)}"
  online.play
  t.assert("Play online leaves the title screen") { !GameMenu.instance.open? }
  t.wait_until(frames: JOIN_FRAMES, message: "could not join the server") { Network.online? }
  t.assert("we are in a room of the server's, with no other player and no hero of the server's") do
    Network.room.to_s.size == CODE_LETTERS && Network.players.empty? && online.others.empty?
  end
  TestInput.leave_note(ROOM_NOTE, Network.room)
  other_done = false
  Network.on(:seen_it_all) do
    other_done = true
    Network.emit(:farewell)
  end

  t.wait_until(frames: JOIN_FRAMES, message: "the other player never arrived") { online.others.size == 1 }
  hero = online.others.values.first
  start = hero.transform.position
  t.wait_until(frames: JOIN_FRAMES, message: "the other player's hero never walked here") do
    hero.transform.position.distance_to(start) > MIN_WALK
  end
  t.screenshot("template_online_first")
  t.wait_until(frames: JOIN_FRAMES, message: "the other player never finished") { other_done }
  t.wait_until(frames: JOIN_FRAMES, message: "the other player never left") { Network.players.empty? }
end
