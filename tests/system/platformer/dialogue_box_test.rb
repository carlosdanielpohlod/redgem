# The hero talks to the old tree through the dialogue box, as a player would: E starts it, Enter
# shows the whole line and then goes on, the arrows move among the choices, a tap on the box goes
# on, and Backspace leaves.
SETTLE_FRAMES = 30
WALK_FRAMES = 120
REACH = 2.0               # meters from the hero to the tree's middle
# Right of the first tree (Blender x = -5.5, y = 1 -> engine x = -5.5, z = -1), looking at it the
# moment it walks left.
START_X = -2.0
TREE_LANE_Z = -1.0
WHOLE_LINE_FRAMES = 240   # long enough for any of the tree's lines to appear by itself

SystemTest.define do |t|
  player = Scene.find("Player")
  tree = Scene.find("Tree1")
  t.wait(SETTLE_FRAMES)
  tree.add_component(Talkable, dialogue: "tree", range: REACH)
  interactor = player.add_component(Interactor)
  box = player.add_component(DialogueBox)
  player.transform.position = Vector3.new(START_X, player.transform.position.y, TREE_LANE_Z)
  t.hold_until(:a, frames: WALK_FRAMES) { !interactor.target.nil? }

  t.press(:e)
  t.wait
  t.assert("E started the conversation") { Dialogue.current&.node_id == "hello" }
  t.assert("the line is still appearing") { !box.whole? }
  t.assert("nothing is in focus while talking") { interactor.target.nil? }
  t.press(:enter)
  t.assert("Enter showed the whole line") { box.whole? && Dialogue.current.node_id == "hello" }
  t.screenshot("dialogue_choices")
  t.press(:down)
  t.press(:up)
  t.press(:enter)
  t.assert("Enter took the choice in focus") { Dialogue.current&.node_id == "thanks" }
  t.assert("the tree was watered") { GameVariables[:tree_watered] == true }
  t.wait(WHOLE_LINE_FRAMES)
  t.assert("a line appears whole by itself") { box.whole? }
  t.click(UI.width / 2, UI.height - DialogueBox::MARGIN - DialogueBox::HEIGHT / 2)
  t.assert("a tap on its last line ended it") { Dialogue.current.nil? }

  t.wait_until(frames: WALK_FRAMES, message: "the tree is not in focus again") { !interactor.target.nil? }
  t.press(:e)
  t.wait
  t.assert("the watered tree starts elsewhere") { Dialogue.current&.node_id == "again" }
  t.wait(WHOLE_LINE_FRAMES)
  t.press(:e)
  t.wait
  t.assert("the E that ended it did not start it again") { Dialogue.current.nil? }
  t.press(:e)
  t.wait
  t.press(:backspace)
  t.assert("Backspace left the conversation") { Dialogue.current.nil? }
end
