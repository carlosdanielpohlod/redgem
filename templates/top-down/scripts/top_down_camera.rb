# The camera above the hero (custom property "target", default "Player"), tilted to look down at
# it from the south, and following it smoothly: the engine's Camera.follow. It never turns, so up
# the screen is always the world's -Z, where W walks.
class TopDownCamera < Behaviour
  DEFAULT_TARGET = "Player".freeze
  HEIGHT = 14.0      # meters above the hero
  DISTANCE = 9.0     # meters south of it (+Z): the camera looks down at about 57 degrees
  SMOOTHING = 0.2    # seconds the camera takes to close half the way to its place

  def start
    offset = Vector3.new(0, prop(:height, HEIGHT), prop(:distance, DISTANCE))
    Camera.follow(find!(prop(:target, DEFAULT_TARGET)), offset: offset, smoothing: SMOOTHING, avoid_walls: false)
  end
end
