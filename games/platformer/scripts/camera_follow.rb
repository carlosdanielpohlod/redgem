# Third-person camera: the engine's Camera.follow, behind and above the hero, smoothed, and in
# front of any wall that would hide the hero. Blender props: target (object name), height,
# distance, smoothing (seconds the camera takes to close half the way to its place).
class CameraFollow < Behaviour
  DEFAULT_TARGET = "Player".freeze
  DEFAULT_HEIGHT = 5.0
  DEFAULT_DISTANCE = 9.0
  DEFAULT_SMOOTHING = 0.14

  def start
    offset = Vector3.new(0, prop(:height, DEFAULT_HEIGHT), prop(:distance, DEFAULT_DISTANCE))
    Camera.follow(find!(prop(:target, DEFAULT_TARGET)), offset: offset, smoothing: prop(:smoothing, DEFAULT_SMOOTHING))
    # Added after the camera started following, it marks the coin where this frame shows it.
    game_object.add_component(CoinMarker)
  end
end
