# Falling off the world sends the hero back to the spawn point, with a shake of the camera.
SETTLE_FRAMES = 20
FALL_FRAMES = 240
SPAWN_TOLERANCE = 0.5
OFF_THE_MAP = Vector3.new(30, 2, 0)

SystemTest.define do |t|
  player = Scene.find("Player")
  t.wait(SETTLE_FRAMES)
  spawn = player.transform.position
  hero = player.get_component(Player)

  player.transform.position = OFF_THE_MAP
  t.wait_until(frames: FALL_FRAMES, message: "hero never respawned") { hero.respawns == 1 }
  t.assert("back at spawn") { player.transform.position.distance_to(spawn) < SPAWN_TOLERANCE }
  t.assert("the fall shakes the camera") { Camera.trauma.positive? }
end
