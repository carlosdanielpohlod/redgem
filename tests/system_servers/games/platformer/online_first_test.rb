# Two players meet on a dedicated server running the platformer (see run_with_server.sh); this one
# arrives first and picks a coin up before the other comes (see online_second_test.rb). Nobody
# hosts: the server keeps the game, and plays nobody. It joins as a guest (the game has no
# backend): the server lets it in under its guest id and keeps its coin for it, which the runner
# finds in the server's save once the server stopped.
SERVER_HOST = "127.0.0.1".freeze
SERVER_NOTE = "server_port".freeze   # where the runner leaves the server's port
ROOM_NOTE = "room".freeze            # where this test leaves the code of its room
COIN_NOTE = "coin_taken".freeze      # ... and says its coin was picked up
IDENTITY_NOTE = "saved_identity".freeze   # ... and who the server should keep that coin for
SETTLE_FRAMES = 30
JOIN_FRAMES = 1800        # the other game may still be starting
WALK_FRAMES = 60
CODE_LETTERS = 6

SystemTest.define do |t|
  t.wait(SETTLE_FRAMES)
  co_op = CoOp.instance
  screen = GameMenu.instance.screens[:multiplayer]
  screen.server = "#{SERVER_HOST}:#{TestInput.note(SERVER_NOTE)}"
  screen.play_online
  t.wait_until(frames: JOIN_FRAMES, message: "could not join the server") { Network.online? }
  t.assert("we are in a room of the server's, and it hosts the game") do
    Network.room.to_s.size == CODE_LETTERS && !Network.host? && !Network.server? && Network.peers == [Network::HOST]
  end
  t.assert("the server plays nobody: no other player, and no hero of its own here") do
    Network.players.empty? && co_op.ghosts.empty?
  end
  TestInput.leave_note(ROOM_NOTE, Network.room)
  TestInput.leave_note(IDENTITY_NOTE, "guest-#{Accounts.guest_id}")

  t.hold(:w, frames: WALK_FRAMES)
  t.wait_until(frames: SETTLE_FRAMES, message: "Coin1 was not collected") { Scene.find("Coin1").nil? }
  t.assert("the coin counts here") { GameManager.instance.coins == 1 }
  TestInput.leave_note(COIN_NOTE, "Coin1")
  other_done = false
  Network.on(:seen_it_all) do
    other_done = true
    Network.emit(:farewell)
  end

  t.wait_until(frames: JOIN_FRAMES, message: "the other player never arrived") { co_op.ghosts.size == 1 }
  t.assert("its hero is another player's") { co_op.ghosts.values.first.get_component(Player).ghost? }
  t.screenshot("online_first")
  t.wait_until(frames: JOIN_FRAMES, message: "the other player never saw the coin gone") { other_done }
  # The other goes once it heard the farewell: nobody's last word is cut off.
  t.wait_until(frames: JOIN_FRAMES, message: "the other player never left") { Network.players.empty? }
end
