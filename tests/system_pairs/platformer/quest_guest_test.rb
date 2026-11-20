# A guest's hero finishes the second stage of the old tree's quest; this is the guest (see
# quest_host_test.rb, which hosts). The host moves the quest; the coin this hero picks up and the
# finish line it reaches are told to the host, which counts them, and this machine hears of it.
HOST_ADDRESS = "127.0.0.1".freeze
PORT_NOTE = "game_port".freeze   # where the host test leaves the port its game is on
SETTLE_FRAMES = 30
JOIN_FRAMES = 1800        # the other game may still be starting

SystemTest.define do |t|
  t.wait(SETTLE_FRAMES)
  co_op = CoOp.instance
  hero = Scene.find("Player")
  hero.add_component(QuestTracker)
  heard = []
  Quests.on(:stage) { |quest, stage, _from| heard << [quest.name, stage] }
  port = 0
  t.wait_until(frames: JOIN_FRAMES, message: "the host never said where its game is") do
    port = TestInput.note(PORT_NOTE).to_i
    port.positive?
  end
  t.assert("the join starts") { Network.join(HOST_ADDRESS, port: port) }
  t.wait_until(frames: JOIN_FRAMES, message: "could not join the host") { Network.online? }
  t.wait_until(frames: JOIN_FRAMES, message: "the host's hero did not appear") { co_op.ghosts.size == 1 }
  t.wait_until(frames: JOIN_FRAMES, message: "the host never moved the quest") { Quests.stage(:tree) == "goal" }
  refused = begin
    Quests.set_stage(:tree, "done")
    false
  rescue ArgumentError
    true
  end
  t.assert("a guest cannot move it") { refused }

  quest = Quests[:tree]
  coin = Scene.find("Coin1")
  hero.transform.position = coin.transform.position
  t.wait_until(frames: JOIN_FRAMES, message: "Coin1 was not collected") { !coin.alive? }
  t.wait_until(frames: JOIN_FRAMES, message: "the host never counted the coin") { quest.objectives.first&.done? }
  hero.transform.position = Scene.find("Goal").transform.position
  t.wait_until(frames: JOIN_FRAMES, message: "the host never completed the quest") { quest.complete? }
  t.assert("this machine heard of each stage from the host") { heard == [%w[tree goal], %w[tree done]] }
  heard_farewell = false
  Network.on(:farewell) { heard_farewell = true }
  Network.emit(:quest_seen)
  t.wait_until(frames: JOIN_FRAMES, message: "the host never said farewell") { heard_farewell }
end
