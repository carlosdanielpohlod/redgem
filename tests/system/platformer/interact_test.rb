# The hero walks up to a tree that can be used: its prompt shows once the tree is near and in
# front, and E (the :interact action) uses it, once.
SETTLE_FRAMES = 30
WALK_FRAMES = 120
REACH = 2.0               # meters from the hero to the tree's middle
FACING = 60.0             # degrees: the hero must look at the tree
# Right of the first tree (Blender x = -5.5, y = 1 -> engine x = -5.5, z = -1), looking at it the
# moment it walks left.
START_X = -2.0
TREE_LANE_Z = -1.0

# Counts what the tree hears.
class TreeUses < Behaviour
  attr_reader :uses, :focused

  def start = @uses = 0
  def on_interact(_actor) = @uses += 1
  def on_focus(_actor) = @focused = true
end

SystemTest.define do |t|
  player = Scene.find("Player")
  tree = Scene.find("Tree1")
  t.wait(SETTLE_FRAMES)
  tree.add_component(Interactable, range: REACH, angle: FACING)
  uses = tree.add_component(TreeUses)
  interactor = player.add_component(Interactor)
  heard = []
  Interaction.on(:interact) { |actor, target| heard << [actor.name, target.name] }
  player.transform.position = Vector3.new(START_X, player.transform.position.y, TREE_LANE_Z)
  t.wait
  t.assert("the tree is out of reach from where the hero starts") { interactor.target.nil? }

  t.hold_until(:a, frames: WALK_FRAMES) { !interactor.target.nil? }
  t.assert("the tree is in focus, and heard of it") { interactor.target.game_object == tree && uses.focused }
  t.assert("its prompt names the key") { interactor.prompt_label(interactor.target) == "[E] Interact" }
  t.screenshot("interact_prompt")
  t.assert("walking up is not using it") { uses.uses.zero? }

  t.press(:e)
  t.wait
  t.assert("E used the tree once") { uses.uses == 1 }
  t.assert("the game heard who used what") { heard == [%w[Player Tree1]] }
end
