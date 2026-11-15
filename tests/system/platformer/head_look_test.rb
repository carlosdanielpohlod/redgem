# An NPC (the first tree, for the test) with a head (a copy of another tree under it, named Head)
# turns that head toward the hero standing in front of it, and back once the hero walks away.
SETTLE_FRAMES = 30
TURN_FRAMES = 30          # more than the look-at's easing
HEAD_HEIGHT = 1.6         # meters above the tree's origin
SIDE = 2.0                # meters right of the tree, and in front of it
AWAY = 20.0               # meters: out of the HeadLook's range
FACING = 0.95             # the cosine of the angle off the hero the head may be at
ALIGNED = 0.999           # the cosine of the angle off straight ahead, once it looks away

SystemTest.define do |t|
  player = Scene.find("Player")
  tree = Scene.find("Tree1")
  t.wait(SETTLE_FRAMES)
  tree.transform.rotation = Vector3.zero  # it faces -Z
  head = Scene.spawn("Tree2", physics: false, parent: tree, name: "Head",
                              position: tree.transform.position + Vector3.up * HEAD_HEIGHT, rotation: Vector3.zero)
  player.add_component(Interactor)
  tree.add_component(HeadLook, range: SIDE * 2, angle: 90.0)
  spot = tree.transform.position + Vector3.new(SIDE, 0, -SIDE)
  player.transform.position = Vector3.new(spot.x, player.transform.position.y, spot.z)
  t.wait(TURN_FRAMES)
  toward = (player.transform.position - head.transform.position).flat.normalized
  t.assert("the head turned toward the hero") { head.transform.forward.flat.normalized.dot(toward) > FACING }
  t.assert("the head's bone is aimed") { tree.animation.looking? }

  player.transform.position = Vector3.new(spot.x + AWAY, player.transform.position.y, spot.z)
  t.wait(TURN_FRAMES)
  t.assert("the head looks straight ahead again") { head.transform.forward.dot(Vector3.forward) > ALIGNED }
  t.assert("and is let go") { !tree.animation.looking? }
end
