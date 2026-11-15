# A guest's hero talks to the old tree and waters it; this is the guest (see talk_host_test.rb,
# which hosts). The host starts the conversation here, as this player's own; the watering is the
# host's to make happen, and this machine hears of it from there.
HOST_ADDRESS = "127.0.0.1".freeze
PORT_NOTE = "game_port".freeze   # where the host test leaves the port its game is on
SETTLE_FRAMES = 30
JOIN_FRAMES = 1800        # the other game may still be starting
SYNC_FRAMES = 90          # for the host to see the hero where it now is
REACH = 2.0               # meters from a hero to the tree's middle
TREE = "Tree1".freeze
# Beside the tree (Blender x = -5.5, y = 1 -> engine x = -5.5, z = -1).
BESIDE_TREE_X = -4.0
TREE_LANE_Z = -1.0

SystemTest.define do |t|
  t.wait(SETTLE_FRAMES)
  co_op = CoOp.instance
  hero = Scene.find("Player")
  tree = Scene.find(TREE)
  tree.add_component(Talkable, dialogue: "tree", range: REACH)
  port = 0
  t.wait_until(frames: JOIN_FRAMES, message: "the host never said where its game is") do
    port = TestInput.note(PORT_NOTE).to_i
    port.positive?
  end
  t.assert("the join starts") { Network.join(HOST_ADDRESS, port: port) }
  t.wait_until(frames: JOIN_FRAMES, message: "could not join the host") { Network.online? }
  t.wait_until(frames: JOIN_FRAMES, message: "the host's hero did not appear") { co_op.ghosts.size == 1 }
  hero.transform.position = Vector3.new(BESIDE_TREE_X, hero.transform.position.y, TREE_LANE_Z)
  t.wait(SYNC_FRAMES)

  t.assert("talking is asked of the host") { Interaction.interact(hero, tree) }
  t.wait_until(frames: JOIN_FRAMES, message: "the host never started the conversation") { Dialogue.current }
  conversation = Dialogue.current
  t.assert("it is the tree's, with this hero") { conversation.node_id == "hello" && conversation.actor == hero }
  t.assert("the tree is who talks") { conversation.speaker == tree }
  t.assert("its effects are not this machine's to make") { !conversation.deciding? }
  t.assert("watering it takes the answer") { conversation.choose(0) }
  t.assert("the tree thanks the hero right away") { conversation.node_id == "thanks" }
  t.wait_until(frames: JOIN_FRAMES, message: "the host never said the tree was watered") do
    GameVariables[:tree_watered] == true
  end
  t.assert("its last line ends it") { conversation.continue && Dialogue.current.nil? }
  heard_farewell = false
  Network.on(:farewell) { heard_farewell = true }
  Network.emit(:talked)
  t.wait_until(frames: JOIN_FRAMES, message: "the host never said farewell") { heard_farewell }
end
