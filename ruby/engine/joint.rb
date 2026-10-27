# What holds two bodies together, or one body to the world. Physics.joint makes them:
#   hinge = Physics.joint(:hinge, door, nil, anchor: hinge_pin, axis: Vector3.up, limits: [0, 110])
#   Physics.joint(:distance, ball, nil, other_anchor: hook)          # a rope from the hook
#   Physics.joint(:fixed, handle, crate, break_force: 500.0)        # comes off when yanked
# A joint goes when either of its objects does (or the scene changes), when `remove` is called,
# and when it is pulled or twisted harder than its break force (then it is `broken?`).
class Joint
  # The objects it holds; `other` is nil for the world.
  attr_reader :kind, :object, :other

  def self.__new(id, kind, object, other)
    joint = allocate
    joint.instance_variable_set(:@id, id)
    joint.instance_variable_set(:@kind, kind.to_sym)
    joint.instance_variable_set(:@object, object)
    joint.instance_variable_set(:@other, other)
    joint
  end

  # A hinge turns at `speed` degrees per second, with at most `torque` newton meters (0: no motor).
  def motor(speed, torque)
    __hinge_only(:motor)
    __motor(speed, torque)
  end

  # How far a hinge turned from where it started, in degrees.
  def angle
    __hinge_only(:angle)
    __angle
  end

  def inspect
    ends = "#{object.name}-#{other ? other.name : "world"}"
    alive? ? "#<Joint #{kind} #{ends}>" : "#<Joint #{kind} #{ends} (gone)>"
  end
  alias to_s inspect

  def __hinge_only(method)
    raise ArgumentError, "only hinges have #{method}, not a #{kind} joint" unless kind == :hinge
  end
end

module Physics
  # What each kind of joint takes besides its objects.
  JOINT_OPTIONS = {
    fixed: %i[anchor break_force],
    hinge: %i[anchor axis limits motor_speed motor_torque break_force],
    slider: %i[anchor axis limits break_force],
    point: %i[anchor break_force],
    distance: %i[anchor other_anchor min max spring damping break_force]
  }.freeze

  # kind is :fixed, :hinge, :slider, :point or :distance; `other` nil joins `object` to the world.
  # Points (anchor:, other_anchor:) and axes are in world space; a hinge's limits: and motor_speed:
  # are in degrees, a slider's limits: in meters from where it starts.
  def self.joint(kind, object, other = nil, **options)
    __refuse_joint_options(kind, options.keys)
    Joint.__new(__joint(kind, object, other, options), kind, object, other)
  end

  # (An unknown kind is left to the engine, which says what kinds there are.)
  def self.__refuse_joint_options(kind, given)
    taken = JOINT_OPTIONS[kind.to_s.to_sym]
    return if taken.nil? || (given - taken).empty?

    raise ArgumentError, "a #{kind} joint takes no #{(given - taken).first.inspect} (it takes #{taken.join(", ")})"
  end
end
