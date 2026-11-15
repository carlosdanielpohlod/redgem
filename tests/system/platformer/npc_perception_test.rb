# A guard (the first tree, for the test) with a Perception and a Brain: the hero walks into its
# sight and the brain hears of it, then a noise the hero makes reaches it.
SETTLE_FRAMES = 30
WALK_FRAMES = 120
SIGHT = 2.5               # meters: the hero starts further away than this
ALL_AROUND = 360.0        # degrees: the tree has no front
EYES = 1.0                # meters above the tree's origin
AIM = 0.5                 # meters above the hero's origin
# Right of the first tree (Blender x = -5.5, y = 1 -> engine x = -5.5, z = -1).
START_X = -2.0
TREE_LANE_Z = -1.0

# Notices the hero, then listens.
class WatchBrain < Brain
  state :idle
  state :alert
  state :listening
  transition from: :idle, to: :alert, on: :see
  transition from: :alert, to: :listening, on: :hear
end

SystemTest.define do |t|
  player = Scene.find("Player")
  tree = Scene.find("Tree1")
  t.wait(SETTLE_FRAMES)
  player.add_component(Interactor)
  perception = tree.add_component(Perception, sight: SIGHT, fov: ALL_AROUND, eyes: EYES, aim: AIM)
  brain = tree.add_component(WatchBrain)
  player.transform.position = Vector3.new(START_X, player.transform.position.y, TREE_LANE_Z)
  t.wait
  t.assert("the hero starts out of the guard's sight") { brain.state == :idle && !perception.sees_any? }

  t.hold_until(:a, frames: WALK_FRAMES) { brain.state == :alert }
  t.assert("the guard saw the hero") { perception.in_sight == [player] }

  heard = Perception.noise(at: player.transform.position, source: player)
  t.assert("the guard heard the hero's noise") { heard == [tree] && brain.state == :listening }
end
