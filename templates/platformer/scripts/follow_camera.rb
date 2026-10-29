# Keeps the camera behind and above its target (custom property "target", default "Player"),
# smoothly, and in front of any wall that would hide it: the engine's Camera.follow.
class FollowCamera < Behaviour
  DEFAULT_TARGET = "Player".freeze
  HEIGHT = 5.0
  DISTANCE = 9.0
  SMOOTHING = 0.14 # seconds the camera takes to close half the way to its place

  def start
    offset = Vector3.new(0, prop(:height, HEIGHT), prop(:distance, DISTANCE))
    Camera.follow(find!(prop(:target, DEFAULT_TARGET)), offset: offset, smoothing: SMOOTHING)
  end
end
