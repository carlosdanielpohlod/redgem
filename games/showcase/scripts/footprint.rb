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
    shortest = nil
    (axes + other.axes).each do |axis_x, axis_z|
      offset = (@x - other.x) * axis_x + (@z - other.z) * axis_z
      overlap = reach(axis_x, axis_z) + other.reach(axis_x, axis_z) - offset.abs
      return nil unless overlap.positive?
      next if shortest && overlap >= shortest[0]

      away = offset.negative? ? -1.0 : 1.0
      shortest = [overlap, axis_x * away, axis_z * away]
    end
    [shortest[1] * shortest[0], shortest[2] * shortest[0]]
  end

  def touches?(other) = !push_out_of(other).nil?

  def axes = [[@along_x, @along_z], [-@along_z, @along_x]]

  # How far the rectangle reaches from its center along a direction.
  def reach(axis_x, axis_z)
    (axis_x * @along_x + axis_z * @along_z).abs * @half_length +
      (axis_z * @along_x - axis_x * @along_z).abs * @half_width
  end
end
