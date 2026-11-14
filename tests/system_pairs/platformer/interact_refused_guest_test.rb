# A guest's hero asks to use a tree from too far away, then from beside it; this is the guest (see
# interact_refused_host_test.rb, which hosts and decides). It asks the way a modified game could:
# straight away, whatever its own Interactor would have picked.
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
  tree.add_component(Interactable, range: REACH)
  port = 0
  t.wait_until(frames: JOIN_FRAMES, message: "the host never said where its game is") do
    port = TestInput.note(PORT_NOTE).to_i
    port.positive?
  end
  t.assert("the join starts") { Network.join(HOST_ADDRESS, port: port) }
  t.wait_until(frames: JOIN_FRAMES, message: "could not join the host") { Network.online? }
  t.wait_until(frames: JOIN_FRAMES, message: "the host's hero did not appear") { co_op.ghosts.size == 1 }
  t.wait(SYNC_FRAMES)

  t.assert("the hero starts out of the tree's reach") { Interaction.distance(hero, tree) > REACH + Interaction::NETWORK_SLACK }
  t.assert("the far use is asked of the host") { Interaction.interact(hero, tree) }
  Network.emit(:asked_from_afar)

  hero.transform.position = Vector3.new(BESIDE_TREE_X, hero.transform.position.y, TREE_LANE_Z)
  t.wait(SYNC_FRAMES)
  t.assert("the near use is asked of the host") { Interaction.interact(hero, tree) }
  heard_farewell = false
  Network.on(:farewell) { heard_farewell = true }
  Network.emit(:asked_from_near)
  t.wait_until(frames: JOIN_FRAMES, message: "the host never said farewell") { heard_farewell }
end
