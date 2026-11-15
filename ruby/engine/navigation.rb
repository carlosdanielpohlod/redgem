# Where the characters a game moves can walk, and the way from one place to another. The scene's
# static bodies are baked into a grid of walkable cells as it loads (when game.json has a
# "navigation" section) or when a script first asks; game.json's section says how big a character
# is (see docs/scripting/gameplay.md#chase-the-player-navigation).
#   waypoints = Navigation.path(transform.position, player.transform.position)
#   spot = Navigation.nearest(clicked_point)
# The bodies that move are not in the grid: a path goes around those it is told to avoid, and a
# character steers around the rest as it walks (Navigation.steer; NavAgent does both).
module Navigation
  DRAW_RADIUS = 10.0 # meters around the point Navigation.draw shows
  DRAW_COLOR = :blue

  # The points to walk through from near `from` to near `to` (each snapped to its nearest walkable
  # point), the first and the last included; nil when there is no way there. avoid: game objects
  # whose bodies the way keeps a character's width off (a crate pushed into a doorway), but one the
  # goal is next to.
  def self.path(from, to, avoid: []) = __path(from, to, Physics.__objects([avoid]))

  # The velocity to walk at among others: the one nearest `preferred` (toward the next waypoint)
  # that keeps clear, for the next few seconds, of the others of `crowd` (each [position, velocity,
  # radius], characters that steer too and take half the avoidance) and of the bodies that move
  # near `position` (but those of `ignore`, the one asking first), and that stays on walkable
  # ground. Flat, and no faster than `speed`.
  def self.steer(position, velocity, preferred, speed:, radius:, crowd: [], ignore: [])
    __steer(position, velocity, preferred, Float(radius), Float(speed), crowd, Physics.__objects([ignore]))
  end

  # The walkable point nearest `point` (within 8 m of it), or nil.
  def self.nearest(point) = __nearest(point)

  # Whether `point` is on walkable ground: over a walkable cell, up to a character's height above it.
  def self.walkable?(point) = __walkable(point)

  # Shows the walkable cells around `around` for this frame (a flat square each), as Debug.box does.
  # Returns how many were drawn.
  def self.draw(around, radius = DRAW_RADIUS, color: DRAW_COLOR, alpha: UI::OPAQUE)
    __draw(around, radius, *UI.rgb(color), alpha)
  end
end
