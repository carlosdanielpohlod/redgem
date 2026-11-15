# A guest's hero talks to the old tree and waters it; this is the host (see talk_guest_test.rb,
# which runs at the same time). The conversation is the guest's own: the host follows it, and the
# watering happens here, from where every machine hears of it.
ANY_PORT = 0              # whichever is free: the guest reads which one it was
PORT_NOTE = "game_port".freeze   # where this test leaves the port for the guest test
SETTLE_FRAMES = 30
JOIN_FRAMES = 1800        # the other game may still be starting
REACH = 2.0               # meters from a hero to the tree's middle
TREE = "Tree1".freeze

SystemTest.define do |t|
  t.wait(SETTLE_FRAMES)
  co_op = CoOp.instance
  Scene.find(TREE).add_component(Talkable, dialogue: "tree", range: REACH)
  watered_by = []
  GameVariables.on_change { |name, value| watered_by << value if name == "tree_watered" }
  t.assert("the game can be hosted") { Network.host(port: ANY_PORT, visible: false) }
  TestInput.leave_note(PORT_NOTE, Network.port.to_s)
  guest_done = false
  Network.on(:talked) { guest_done = true }

  t.wait_until(frames: JOIN_FRAMES, message: "nobody joined") { co_op.ghosts.size == 1 }
  peer = co_op.ghosts.keys.first
  t.wait_until(frames: JOIN_FRAMES, message: "the guest never talked to the tree") { Dialogue.__mirror_of(peer) }
  t.assert("the conversation is not the host's player's") { Dialogue.current.nil? }
  t.wait_until(frames: JOIN_FRAMES, message: "the watering never happened here") { watered_by == [true] }
  t.assert("the host counted it once") { GameVariables[:tree_waterings] == 1 }
  t.wait_until(frames: JOIN_FRAMES, message: "the guest never said it was done") { guest_done }
  t.assert("the host forgot the conversation that ended") { Dialogue.__mirror_of(peer).nil? }
  Network.emit(:farewell)
  # The guest goes once it heard the farewell, and the game closes after it.
  t.wait_until(frames: JOIN_FRAMES, message: "the guest never left") { Network.peers.empty? }
end
