# 3D vector used across the scripting API. The engine makes the class (its three numbers live
# inside each object) and its arithmetic: x, y, z, +, -, unary -, * and / by a number, dot,
# cross, length, length_squared, normalized and ==, each of which makes a new Vector3; and
# set(x, y, z), add!, sub!, scale! and normalize!, which change the vector they are called on and
# hand it back (for math done thousands of times a frame). What is built on those is here.
class Vector3
  EPSILON = 1e-6

  def self.[](x, y, z)
    new(x, y, z)
  end

  # Converts Blender coordinates (Z up, +Y forward) to engine coordinates (Y up, -Z forward).
  def self.blender(x, y, z) = new(x, z, -y)

  def self.zero = new(0, 0, 0)
  def self.one = new(1, 1, 1)
  def self.up = new(0, 1, 0)
  def self.down = new(0, -1, 0)
  def self.right = new(1, 0, 0)
  def self.left = new(-1, 0, 0)
  # Objects and cameras look down -Z (glTF convention).
  def self.forward = new(0, 0, -1)
  def self.back = new(0, 0, 1)

  # The same direction, no longer than max_length: a half-pushed stick walks, a full one runs,
  # and two keys at once are no faster than one.
  def clamped(max_length = 1.0)
    size = length
    size > max_length ? self * (max_length / size) : self
  end

  def distance_to(other) = (other - self).length

  # Same vector with y = 0, handy for ground movement.
  def flat = Vector3.new(x, 0, z)

  def with(x: self.x, y: self.y, z: self.z) = Vector3.new(x, y, z)

  def lerp(other, t) = self + (other - self) * t

  def approx?(other, tolerance = EPSILON) = (self - other).length <= tolerance

  def to_a = [x, y, z]

  def to_s = format("(%<x>.3f, %<y>.3f, %<z>.3f)", x: x, y: y, z: z)
  def inspect = "Vector3#{self}"
end
