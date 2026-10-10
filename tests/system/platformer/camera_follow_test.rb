# The camera keeps following and looking at the hero while it moves.
WALK_FRAMES = 90
MAX_DISTANCE = 15.0
MIN_FACING = 0.9

SystemTest.define do |t|
  camera = Camera.main
  player = Scene.find("Player")
  t.assert("the Blender camera is the main camera") { camera.name == "Camera" }

  t.hold(:a, frames: WALK_FRAMES)
  to_player = (player.transform.position - camera.transform.position)
  t.assert("camera stays close") { to_player.length < MAX_DISTANCE }
  t.assert("camera looks at the hero") { camera.transform.forward.dot(to_player.normalized) > MIN_FACING }
  t.screenshot("camera_follow")
end
