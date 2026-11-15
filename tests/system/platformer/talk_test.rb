# The hero walks up to the old tree and talks to it: E starts the tree's dialogue
# (games/platformer/dialogues/tree.json), watering it changes the game's variables, and the
# next talk starts where the dialogue says for a tree that was watered.
SETTLE_FRAMES = 30
WALK_FRAMES = 120
REACH = 2.0               # meters from the hero to the tree's middle
# Right of the first tree (Blender x = -5.5, y = 1 -> engine x = -5.5, z = -1), looking at it the
# moment it walks left.
START_X = -2.0
TREE_LANE_Z = -1.0

SystemTest.define do |t|
  player = Scene.find("Player")
  tree = Scene.find("Tree1")
  t.wait(SETTLE_FRAMES)
  tree.add_component(Talkable, dialogue: "tree", range: REACH)
  interactor = player.add_component(Interactor)
  lines = []
  Dialogue.on(:line) { |conversation| lines << conversation.node_id }
  player.transform.position = Vector3.new(START_X, player.transform.position.y, TREE_LANE_Z)
  t.hold_until(:a, frames: WALK_FRAMES) { !interactor.target.nil? }
  t.assert("its prompt says Talk") { interactor.prompt_label(interactor.target) == "[E] Talk" }

  t.press(:e)
  t.wait
  conversation = Dialogue.current
  t.assert("E started the tree's dialogue") { conversation&.node_id == "hello" }
  t.assert("the tree is who talks") { conversation.speaker == tree && conversation.speaker_name == "Old tree" }
  t.assert("the hero is who it talks to") { conversation.actor == player }
  t.assert("both answers are offered") { conversation.choices.map(&:text) == ["Water the tree", "Goodbye"] }
  t.assert("watering it takes the answer") { conversation.choose(conversation.choices.first) }
  t.assert("its effects happened") { GameVariables[:tree_watered] == true && GameVariables[:tree_waterings] == 1 }
  t.assert("the tree thanks the hero") { conversation.text == "Ahh, much better. I will remember this." }
  t.assert("its last line ends it") { conversation.continue && conversation.over? && Dialogue.current.nil? }
  t.press(:e)
  t.wait
  t.assert("a press on the frame it ended uses nothing") { Dialogue.current.nil? }

  t.wait
  t.press(:e)
  t.wait
  t.assert("a watered tree starts elsewhere") { Dialogue.current&.node_id == "again" }
  t.assert("the player heard of every line") { lines == %w[hello thanks again] }
end
