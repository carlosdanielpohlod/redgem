# Two players play the multiplayer-levels template's quest on a dedicated server (see
# run_with_server.sh); this one comes second, into the first one's room. It goes where the first
# player's doors take the party, reaches the beacon and tells the guide (see levels_first_test.rb).
SERVER_HOST = "127.0.0.1".freeze
SERVER_NOTE = "server_port".freeze   # where the runner leaves the server's port
ROOM_NOTE = "room".freeze            # where the first test leaves the code of its room
SETTLE_FRAMES = 30
WAIT_SECONDS = 60                    # for what the server and the other game do
PRESS_SECONDS = 0.5                  # between presses of E, while the server sees the hero arrive
CODE_LETTERS = 6
NEAR = Vector3.new(0, 0, 1.5)        # where a hero stands to use something: in front of it, in reach
ON_POINT = 0.1                       # meters from its spawn point a hero that was put there is
POINTS = 2                           # the spawn points of a level: Spawn1 and Spawn2

SystemTest.define do |t|
  t.wait(SETTLE_FRAMES)
  GameMenu.instance.resume
  room = nil
  t.wait_for(seconds: WAIT_SECONDS, message: "the first player never got in") do
    room = TestInput.note(ROOM_NOTE).to_s
    room.size == CODE_LETTERS
  end
  t.assert("the join starts") { Session.play_on_server("#{SERVER_HOST}:#{TestInput.note(SERVER_NOTE)}") }
  t.wait_for(seconds: WAIT_SECONDS, message: "could not join the server") { Network.online? }
  t.assert("quick play put us in the first player's room") { Network.room == room }
  t.wait_for(seconds: WAIT_SECONDS, message: "the first player's hero did not appear") do
    Party.instance.teammates.size == 1
  end

  hero_on_its_point = lambda do
    Scene.find("Player").transform.position.flat.distance_to(Spawns.mine.transform.position.flat) < ON_POINT
  end
  t.assert("the hero is on its own spawn point, not the first player's") do
    hero_on_its_point.call && Spawns.index(Network.id, POINTS) != Spawns.index(Network.players.first, POINTS)
  end
  arrive = lambda do |level|
    t.wait_for(seconds: WAIT_SECONDS, message: "never got to #{level}") do
      Game.scene_name == level && !Network.changing_level?
    end
    t.wait(SETTLE_FRAMES)
  end

  arrive.call("level2")
  t.assert("the hero is on its own spawn point of level2") { hero_on_its_point.call }
  t.assert("the quest came along") { Quests.stage(:beacon) == "find" }
  Scene.find("Player").transform.position = Scene.find("Beacon").transform.position + NEAR
  t.wait_for(seconds: WAIT_SECONDS, message: "the server never counted the beacon") do
    Quests.stage(:beacon) == "report"
  end

  arrive.call("main")
  Scene.find("Player").transform.position = Scene.find("Guide").transform.position + NEAR
  pressed_at = 0.0
  t.wait_for(seconds: WAIT_SECONDS, message: "no conversation started") do
    pressed_at = Time.now.to_f.tap { t.press(:e) } if Time.now.to_f - pressed_at > PRESS_SECONDS
    Dialogue.current
  end
  Dialogue.current.choosing? ? Dialogue.current.choose(0) : Dialogue.current.continue while Dialogue.current
  t.wait_for(seconds: WAIT_SECONDS, message: "the server never completed the quest") { Quests[:beacon].complete? }

  heard_farewell = false
  Network.on(:farewell) { heard_farewell = true }
  Network.emit(:seen_it_all)
  t.wait_for(seconds: WAIT_SECONDS, message: "the first player never said farewell") { heard_farewell }
end
