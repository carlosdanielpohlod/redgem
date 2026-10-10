# The kinematic platform moves along Blender's X axis and carries the hero standing on it.
OBSERVE_FRAMES = 30
RIDE_FRAMES = 90
MIN_TRAVEL = 0.5
STAND_HEIGHT = 1.2

SystemTest.define do |t|
  platform = Scene.find("MovingPlatform")
  player = Scene.find("Player")
  start = platform.transform.position

  t.wait(OBSERVE_FRAMES)
  t.assert("platform moved sideways") { (platform.transform.position.x - start.x).abs > MIN_TRAVEL }
  t.assert("platform kept its height and depth") do
    platform.transform.position.y == start.y && platform.transform.position.z == start.z
  end

  # Drop the hero onto the platform and let it ride.
  player.transform.position = platform.transform.position + Vector3.up * STAND_HEIGHT
  t.wait_until(frames: OBSERVE_FRAMES, message: "hero never landed on the platform") { player.character.grounded? }
  offset = player.transform.position.x - platform.transform.position.x
  t.wait(RIDE_FRAMES)
  t.assert("hero rides along with the platform") do
    (player.transform.position.x - platform.transform.position.x - offset).abs < MIN_TRAVEL
  end
  t.screenshot("moving_platform_ride")
end
