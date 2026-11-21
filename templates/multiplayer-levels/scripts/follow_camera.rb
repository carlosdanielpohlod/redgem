# Keeps the camera behind and above the hero (custom property "target", default "Player"),
# smoothly: the engine's Camera.follow. On a dedicated server there is no hero to follow.
class FollowCamera < Behaviour
  DEFAULT_TARGET = "Player".freeze
  HEIGHT = 8.0
  DISTANCE = 11.0
  SMOOTHING = 0.14 # seconds the camera takes to close half the way to its place

  def start
    return if Network.server?

    target = find!(prop(:target, DEFAULT_TARGET))
    Camera.follow(target, offset: Vector3.new(0, HEIGHT, DISTANCE), smoothing: SMOOTHING)
  end
end
