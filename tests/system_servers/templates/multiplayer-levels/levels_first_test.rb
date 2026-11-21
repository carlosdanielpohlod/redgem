# Two players play the multiplayer-levels template's quest on a dedicated server (see
# run_with_server.sh), with nobody hosting; this one arrives first. It takes the quest from the
# guide and uses the doors: the server runs the use and takes both players to the other level.
# The other player reaches the beacon and tells the guide (see levels_second_test.rb).
SERVER_HOST = "127.0.0.1".freeze
SERVER_NOTE = "server_port".freeze   # where the runner leaves the server's port
ROOM_NOTE = "room".freeze            # where this test leaves the code of its room
SETTLE_FRAMES = 30
WAIT_SECONDS = 60                    # for what the server and the other game do
PRESS_SECONDS = 0.5                  # between presses of E, while the server sees the hero arrive
NEAR = Vector3.new(0, 0, 1.5)        # where a hero stands to use something: in front of it, in reach
ON_POINT = 0.1                       # meters from its spawn point a hero that was put there is

SystemTest.define do |t|
  t.wait(SETTLE_FRAMES)
  GameMenu.instance.resume
  t.assert("the join starts") { Session.play_on_server("#{SERVER_HOST}:#{TestInput.note(SERVER_NOTE)}") }
  t.wait_for(seconds: WAIT_SECONDS, message: "could not join the server") { Network.online? }
  TestInput.leave_note(ROOM_NOTE, Network.room)
  t.wait_for(seconds: WAIT_SECONDS, message: "the other player never arrived") { Party.instance.teammates.size == 1 }
  t.assert("the hero is on its own spawn point") do
    Scene.find("Player").transform.position.flat.distance_to(Spawns.mine.transform.position.flat) < ON_POINT
  end

  # Walks up to something and presses E now and then, until `done` holds.
  use = lambda do |name, message, &done|
    Scene.find("Player").transform.position = Scene.find(name).transform.position + NEAR
    pressed_at = 0.0
    t.wait_for(seconds: WAIT_SECONDS, message: message) do
      pressed_at = Time.now.to_f.tap { t.press(:e) } if Time.now.to_f - pressed_at > PRESS_SECONDS
      done.call
    end
  end
  travel = lambda do |level|
    use.call("Exit", "the door never took the party to #{level}") do
      Game.scene_name == level && !Network.changing_level?
    end
    t.wait(SETTLE_FRAMES)
  end

  use.call("Guide", "no conversation started") { Dialogue.current }
  Dialogue.current.choosing? ? Dialogue.current.choose(0) : Dialogue.current.continue while Dialogue.current
  t.wait_for(seconds: WAIT_SECONDS, message: "the server never moved the quest") { Quests.stage(:beacon) == "find" }

  travel.call("level2")
  t.assert("both players are still in the room") { Network.online? && Network.players.size == 1 }
  t.screenshot("multiplayer_levels_server_level2")
  t.wait_for(seconds: WAIT_SECONDS, message: "the other player never reached the beacon") do
    Quests.stage(:beacon) == "report"
  end

  travel.call("main")
  other_done = false
  Network.on(:seen_it_all) do   # (handlers last until a level loads: once back)
    other_done = true
    Network.emit(:farewell)
  end
  t.wait_for(seconds: WAIT_SECONDS, message: "the quest was never completed") { Quests[:beacon].complete? }
  t.wait_for(seconds: WAIT_SECONDS, message: "the other player never finished") { other_done }
  t.wait_for(seconds: WAIT_SECONDS, message: "the other player never left") { Network.players.empty? }
end
