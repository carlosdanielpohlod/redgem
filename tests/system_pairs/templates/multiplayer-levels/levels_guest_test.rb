# Two players play the multiplayer-levels template's quest together, through both levels; this is
# the guest (see levels_host_test.rb, which hosts). It talks to the guide (the conversation shows
# here, the host follows it and moves the quest), goes where the host's doors take the party,
# reaches the beacon and tells the guide.
HOST_ADDRESS = "127.0.0.1".freeze
PORT_NOTE = "game_port".freeze    # where the host test leaves the port its game is on
SETTLE_FRAMES = 30
WAIT_SECONDS = 60                 # for what the other game does (it may still be starting)
TALK_SECONDS = 0.5                # between presses of E, while the host sees the hero arrive
NEAR = Vector3.new(0, 0, 1.5)     # where a hero stands to use something: in front of it, in reach
ON_POINT = 0.1                    # meters from its spawn point a hero that was put there is

SystemTest.define do |t|
  t.wait(SETTLE_FRAMES)
  GameMenu.instance.resume
  port = 0
  t.wait_for(seconds: WAIT_SECONDS, message: "the host never said where its game is") do
    port = TestInput.note(PORT_NOTE).to_i
    port.positive?
  end
  t.assert("the join starts") { Session.join(HOST_ADDRESS, port: port) }
  t.wait_for(seconds: WAIT_SECONDS, message: "could not join the host") { Network.online? }
  t.wait_for(seconds: WAIT_SECONDS, message: "the host's hero did not appear") { Party.instance.teammates.size == 1 }

  on_spawn = lambda do
    Scene.find("Player").transform.position.flat.distance_to(Scene.find("Spawn2").transform.position.flat) < ON_POINT
  end
  # Walks up to the guide and goes through the conversation, taking the first choice it offers. E
  # is pressed again now and then: the host sees the hero arrive a moment late.
  talk = lambda do
    Scene.find("Player").transform.position = Scene.find("Guide").transform.position + NEAR
    asked_at = 0.0
    t.wait_for(seconds: WAIT_SECONDS, message: "no conversation started") do
      asked_at = Time.now.to_f.tap { t.press(:e) } if Time.now.to_f - asked_at > TALK_SECONDS
      Dialogue.current
    end
    Dialogue.current.choosing? ? Dialogue.current.choose(0) : Dialogue.current.continue while Dialogue.current
  end
  # Waits until the host's door took the party to that level, and it started here.
  arrive = lambda do |level|
    t.wait_for(seconds: WAIT_SECONDS, message: "never got to #{level}") do
      Game.scene_name == level && !Network.changing_level?
    end
    t.wait(SETTLE_FRAMES)
  end

  t.assert("the guest's hero is on Spawn2") { on_spawn.call }
  talk.call
  t.wait_for(seconds: WAIT_SECONDS, message: "the host never moved the quest") { Quests.stage(:beacon) == "find" }

  arrive.call("level2")
  t.assert("the guest's hero is on Spawn2 of level2") { on_spawn.call }
  t.assert("the quest came along") { Quests.stage(:beacon) == "find" }
  t.screenshot("multiplayer_levels_guest_level2")
  Scene.find("Player").transform.position = Scene.find("Beacon").transform.position + NEAR
  t.wait_for(seconds: WAIT_SECONDS, message: "the host never counted the beacon") do
    Quests.stage(:beacon) == "report"
  end

  arrive.call("main")
  talk.call
  t.wait_for(seconds: WAIT_SECONDS, message: "the host never completed the quest") { Quests[:beacon].complete? }
  heard_farewell = false
  Network.on(:farewell) { heard_farewell = true }
  Network.emit(:quest_seen)
  t.wait_for(seconds: WAIT_SECONDS, message: "the host never said farewell") { heard_farewell }
end
