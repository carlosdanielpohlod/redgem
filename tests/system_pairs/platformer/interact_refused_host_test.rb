# A guest's hero asks to use a tree from too far away, then from beside it; this is the host, which
# decides (see interact_refused_guest_test.rb, which runs at the same time). The far use is refused
# here, where the guest's hero is seen: only the near one happens.
ANY_PORT = 0              # whichever is free: the guest reads which one it was
PORT_NOTE = "game_port".freeze   # where this test leaves the port for the guest test
SETTLE_FRAMES = 30
JOIN_FRAMES = 1800        # the other game may still be starting
REACH = 2.0               # meters from a hero to the tree's middle
TREE = "Tree1".freeze

# Writes down who used the tree.
class TreeUsers < Behaviour
  attr_reader :users

  def start = @users = []
  def on_interact(actor) = @users << actor
end

SystemTest.define do |t|
  t.wait(SETTLE_FRAMES)
  co_op = CoOp.instance
  Scene.find(TREE).add_component(Interactable, range: REACH)
  tree = Scene.find(TREE).add_component(TreeUsers)
  t.assert("the game can be hosted") { Network.host(port: ANY_PORT, visible: false) }
  TestInput.leave_note(PORT_NOTE, Network.port.to_s)
  asked_from_afar = false
  Network.on(:asked_from_afar) { asked_from_afar = true }
  guest_done = false
  Network.on(:asked_from_near) { guest_done = true }

  t.wait_until(frames: JOIN_FRAMES, message: "nobody joined") { co_op.ghosts.size == 1 }
  ghost = co_op.ghosts.values.first
  t.wait_until(frames: JOIN_FRAMES, message: "the guest never asked from afar") { asked_from_afar }
  t.assert("the use asked from afar was refused") { tree.users.empty? }
  t.wait_until(frames: JOIN_FRAMES, message: "the guest never asked from beside the tree") { guest_done }
  t.wait_until(frames: SETTLE_FRAMES, message: "the near use did not happen") { tree.users.size == 1 }
  t.assert("it was the guest's hero that used the tree") { tree.users == [ghost] }
  Network.emit(:farewell)
  # The guest goes once it heard the farewell, and the game closes after it.
  t.wait_until(frames: JOIN_FRAMES, message: "the guest never left") { Network.peers.empty? }
end
