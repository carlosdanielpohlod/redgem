# The camera follows the hero (Camera.follow), a marker floats over the coin nearest the camera
# where the camera shows it (Camera.world_to_screen), and a fall shakes the camera, which then
# comes back to where it follows the hero. (tests/render_system/camera_marker_render_test.cpp
# checks the picture has the marker over the coin.)
SETTLE_FRAMES = 60
SHAKE_FRAMES = 90
PIXELS = 1.0       # how far apart the marker and the coin may be across the screen
METERS = 0.05      # how far from its place the camera may be once the shake is over

SystemTest.define do |t|
  t.wait(SETTLE_FRAMES)
  marker = Camera.main.get_component(CoinMarker)
  t.assert("the camera follows the hero") { Camera.following? }
  t.assert("the marker is on the camera") { !marker.nil? }
  t.assert("it marks the coin nearest the camera") do
    camera = Camera.main.transform.position
    marker.coin == Scene.find_all(:coin).min_by { |coin| coin.transform.position.distance_to(camera) }
  end
  t.assert("it floats over the coin, on the screen") do
    x, y = marker.spot
    coin_x, coin_y = Camera.world_to_screen(marker.coin)
    (x - coin_x).abs < PIXELS && y < coin_y && x.between?(0, UI.width) && y.between?(0, UI.height)
  end
  t.screenshot("coin_marker")

  followed = Camera.main.transform.position
  Scene.find("Player").get_component(Player).respawn
  t.wait
  t.assert("a fall shakes the camera") { Camera.trauma.positive? }
  t.screenshot("fall_shake")
  t.wait(SHAKE_FRAMES)
  t.assert("the shake is over") { Camera.trauma.zero? }
  t.assert("the camera is back where it follows the hero") do
    Camera.main.transform.position.distance_to(followed) < METERS
  end
end
