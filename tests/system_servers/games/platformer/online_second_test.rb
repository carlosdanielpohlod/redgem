# Two players meet on a dedicated server running the platformer (see run_with_server.sh); this one
# comes second, once the first picked a coin up (see online_first_test.rb). Quick play puts it in
# the first one's room, and the server, which kept the game meanwhile, tells it the coin is gone.
SERVER_HOST = "127.0.0.1".freeze
SERVER_NOTE = "server_port".freeze   # where the runner leaves the server's port
ROOM_NOTE = "room".freeze            # where the first test leaves the code of its room
COIN_NOTE = "coin_taken".freeze      # ... and the coin it picked up
SETTLE_FRAMES = 30
JOIN_FRAMES = 1800        # the other game may still be starting
CODE_LETTERS = 6

SystemTest.define do |t|
  t.wait(SETTLE_FRAMES)
  co_op = CoOp.instance
  manager = GameManager.instance
  room = nil
  t.wait_until(frames: JOIN_FRAMES, message: "the first player never got in") do
    room = TestInput.note(ROOM_NOTE).to_s
    room.size == CODE_LETTERS
  end
  t.wait_until(frames: JOIN_FRAMES, message: "the first player's coin was never picked up") do
    TestInput.note(COIN_NOTE).to_s == "Coin1"
  end
  t.assert("the coin is still here before we join") { Scene.find("Coin1") }

  screen = GameMenu.instance.screens[:multiplayer]
  screen.server = "#{SERVER_HOST}:#{TestInput.note(SERVER_NOTE)}"
  screen.play_online
  t.wait_until(frames: JOIN_FRAMES, message: "could not join the server") { Network.online? }
  t.assert("quick play put us in the first player's room") { Network.room == room && Network.players.size == 1 }
  t.wait_until(frames: JOIN_FRAMES, message: "the server never said the coin is gone") { Scene.find("Coin1").nil? }
  t.assert("the server kept count of the coin taken before we came") do
    manager.coins == 1 && manager.collected == ["Coin1"]
  end
  t.wait_until(frames: SETTLE_FRAMES, message: "the first player's hero did not appear") { co_op.ghosts.size == 1 }
  t.assert("the one hero we see is the first player's: the server has none") { co_op.ghosts.keys == Network.players }
  t.screenshot("online_second")

  # The first is told, and its answer waited for: the game is not closed on a message still on its way.
  heard_farewell = false
  Network.on(:farewell) { heard_farewell = true }
  Network.emit(:seen_it_all)
  t.wait_until(frames: JOIN_FRAMES, message: "the first player never said farewell") { heard_farewell }
end
