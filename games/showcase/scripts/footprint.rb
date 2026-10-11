# The rectangle a car covers on the ground, turned the way the car points. Cars use it to keep
# from driving into each other.
class Footprint
  attr_reader :x, :z, :half_length, :half_width

  # (along_x, along_z): the unit vector along the rectangle's length.
  def initialize(x:, z:, along_x:, along_z:, half_length:, half_width:)
    @x = x
    @z = z
    @along_x = along_x
    @along_z = along_z
    @half_length = half_length
    @half_width = half_width
  end

  # Half the size of the street-aligned box around it: how far it reaches across (X) and along (Z).
  def half_extent_x = @along_x.abs * @half_length + @along_z.abs * @half_width
  def half_extent_z = @along_z.abs * @half_length + @along_x.abs * @half_width

  # The shortest move [dx, dz] that takes this rectangle out of `other`; nil when they do not touch.
  # (Separating axes: two rectangles are apart when their shadows on one of their sides' directions
  # do not overlap.)
  def push_out_of(other)
    overlaps = []
    (axes + other.axes).each do |axis_x, axis_z|
      overlaps << overlap_along(axis_x, axis_z, other)
      return nil unless overlaps.last.first.positive?
    end
    depth, away_x, away_z = overlaps.min_by(&:first)
    [away_x * depth, away_z * depth]
  end

  # How deep the two rectangles are into each other along a direction, and which way along it
  # leads this one out: [depth, away_x, away_z]. They are apart when the depth is not positive.
  def overlap_along(axis_x, axis_z, other)
    offset = (@x - other.x) * axis_x + (@z - other.z) * axis_z
    away = offset.negative? ? -1.0 : 1.0
    [reach(axis_x, axis_z) + other.reach(axis_x, axis_z) - offset.abs, axis_x * away, axis_z * away]
  end

  def touches?(other) = !push_out_of(other).nil?

  def axes = [[@along_x, @along_z], [-@along_z, @along_x]]

  # How far the rectangle reaches from its center along a direction.
  def reach(axis_x, axis_z)
    (axis_x * @along_x + axis_z * @along_z).abs * @half_length +
      (axis_z * @along_x - axis_x * @along_z).abs * @half_width
  end
end
