# Immutable-style 3D vector used across the scripting API.
class Vector3
  attr_reader :x, :y, :z

  EPSILON = 1e-6

  def initialize(x = 0.0, y = 0.0, z = 0.0)
    @x = x.to_f
    @y = y.to_f
    @z = z.to_f
  end

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

  def +(other) = Vector3.new(@x + other.x, @y + other.y, @z + other.z)
  def -(other) = Vector3.new(@x - other.x, @y - other.y, @z - other.z)
  def -@ = Vector3.new(-@x, -@y, -@z)
  def *(scalar) = Vector3.new(@x * scalar, @y * scalar, @z * scalar)
  def /(scalar) = Vector3.new(@x / scalar, @y / scalar, @z / scalar)

  def dot(other) = @x * other.x + @y * other.y + @z * other.z

  def cross(other)
    Vector3.new(@y * other.z - @z * other.y, @z * other.x - @x * other.z, @x * other.y - @y * other.x)
  end

  def length = Math.sqrt(dot(self))
  def length_squared = dot(self)

  def normalized
    len = length
    len > EPSILON ? self / len : Vector3.zero
  end

  # The same direction, no longer than max_length: a half-pushed stick walks, a full one runs,
  # and two keys at once are no faster than one.
  def clamped(max_length = 1.0)
    size = length
    size > max_length ? self * (max_length / size) : self
  end

  def distance_to(other) = (other - self).length

  # Same vector with y = 0, handy for ground movement.
  def flat = Vector3.new(@x, 0, @z)

  def with(x: @x, y: @y, z: @z) = Vector3.new(x, y, z)

  def lerp(other, t) = self + (other - self) * t

  def ==(other)
    other.is_a?(Vector3) && @x == other.x && @y == other.y && @z == other.z
  end

  def approx?(other, tolerance = EPSILON) = (self - other).length <= tolerance

  def to_a = [@x, @y, @z]

  def to_s = format("(%.3f, %.3f, %.3f)", @x, @y, @z)
  def inspect = "Vector3#{to_s}"
end
