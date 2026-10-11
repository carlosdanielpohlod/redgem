# The holes in the road: the scene's objects tagged "pothole", each with its `radius` and `depth`
# in meters. A car asks how far it sinks where it is.
class Potholes
  TAG = "pothole".freeze
  FOLLOWED = 0.7   # share of a pothole's depth the car body follows (the suspension takes the rest)

  def initialize
    @holes = Scene.find_all(TAG).map do |hole|
      [hole.transform.position.x, hole.transform.position.z, hole.props[:radius].to_f, hole.props[:depth].to_f]
    end
  end

  # How far a car sinks at a position: the deepest pothole under it, eased toward its edge.
  def drop_at(position)
    @holes.map { |hole| drop_into(hole, position) }.max || 0.0
  end

  private

  def drop_into(hole, position)
    x, z, radius, depth = hole
    distance = Math.sqrt((position.x - x)**2 + (position.z - z)**2)
    distance < radius ? depth * FOLLOWED * (1.0 - distance / radius) : 0.0
  end
end
