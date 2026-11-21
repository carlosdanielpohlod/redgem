# Two players play the multiplayer-levels template's quest together, through both levels; this is
# the host (see levels_guest_test.rb, which joins it). The guest takes the quest from the guide
# and does it; the host takes the party from level to level with the doors, and keeps the save:
# the quest and the level survive each change.
ANY_PORT = 0                      # whichever is free: the guest reads which one it was
PORT_NOTE = "game_port".freeze    # where this test leaves the port for the guest test
SETTLE_FRAMES = 30
WAIT_SECONDS = 60                 # for what the other game does (it may still be starting)
NEAR = Vector3.new(0, 0, 1.5)     # where a hero stands to use something: in front of it, in reach
ON_POINT = 0.1                    # meters from its spawn point a hero that was put there is

SystemTest.define do |t|
  t.wait(SETTLE_FRAMES)
  GameMenu.instance.resume
  Progress.clear
  t.assert("the game can be hosted") { Session.host(port: ANY_PORT, visible: false) }
  TestInput.leave_note(PORT_NOTE, Network.port.to_s)

  on_spawn = lambda do
    Scene.find("Player").transform.position.flat.distance_to(Scene.find("Spawn1").transform.position.flat) < ON_POINT
  end
  # Takes the party through the door of the level that runs, and waits until it plays the next one.
  travel = lambda do |to|
    Scene.find("Player").transform.position = Scene.find("Exit").transform.position + NEAR
    t.wait
    t.press(:e)
    t.wait_for(seconds: WAIT_SECONDS, message: "the party never got to #{to}") do
      Game.scene_name == to && !Network.changing_level?
    end
    t.wait_for(seconds: WAIT_SECONDS, message: "the guest's hero is not in #{to}") do
      Party.instance.teammates.size == 1
    end
  end

  t.wait_for(seconds: WAIT_SECONDS, message: "nobody joined") { Party.instance.teammates.size == 1 }
  t.assert("the host's hero is on Spawn1") { on_spawn.call }
  t.wait_for(seconds: WAIT_SECONDS, message: "the guest never took the quest") { Quests.stage(:beacon) == "find" }
  t.assert("the host saved it") { Save.get(:quests)["beacon"]["stage"] == "find" }

  travel.call("level2")
  t.assert("both machines are still in the game") { Network.online? && Network.peers.size == 1 }
  t.assert("the host's hero is on Spawn1 of level2") { on_spawn.call }
  t.assert("the save survived the change and says the new level") do
    Progress.level == "level2" && Save.get(:quests)["beacon"]["stage"] == "find"
  end
  t.screenshot("multiplayer_levels_host_level2")
  t.wait_for(seconds: WAIT_SECONDS, message: "the guest never reached the beacon") do
    Quests.stage(:beacon) == "report"
  end

  travel.call("main")
  guest_done = false
  Network.on(:quest_seen) { guest_done = true }   # (handlers last until a level loads: once back)
  t.wait_for(seconds: WAIT_SECONDS, message: "the guest never told the guide") { Quests[:beacon].complete? }
  t.assert("the save says the quest is done") { Save.get(:quests)["beacon"]["stage"] == "done" }
  t.wait_for(seconds: WAIT_SECONDS, message: "the guest never saw it complete") { guest_done }
  Network.emit(:farewell)
  # The guest goes once it heard the farewell, and the game closes after it.
  t.wait_for(seconds: WAIT_SECONDS, message: "the guest never left") { Network.peers.empty? }
end
