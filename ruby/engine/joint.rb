# What holds two bodies together, or one body to the world. Physics.joint makes them:
#   hinge = Physics.joint(:hinge, door, nil, anchor: hinge_pin, axis: Vector3.up, limits: [0, 110])
#   Physics.joint(:distance, ball, nil, other_anchor: hook)          # a rope from the hook
#   Physics.joint(:fixed, handle, crate, break_force: 500.0)        # comes off when yanked
# A joint goes when either of its objects does (or the scene changes), when `remove` is called,
# and when it is pulled harder than its break force (then it is `broken?`).
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

  def alive? = object.alive? && __alive?

  def inspect
    ends = "#{object.name}-#{other ? other.name : "world"}"
    alive? ? "#<Joint #{kind} #{ends}>" : "#<Joint #{kind} #{ends} (gone)>"
  end
  alias to_s inspect
end

module Physics
  JOINT_OPTIONS = %i[anchor other_anchor axis limits motor_speed motor_torque min max spring damping
                     break_force].freeze

  # kind is :fixed, :hinge, :slider, :point or :distance; `other` nil joins `object` to the world.
  # Points (anchor:, other_anchor:) and axes are in world space; a hinge's limits: and motor_speed:
  # are in degrees, a slider's limits: in meters from where it starts.
  def self.joint(kind, object, other = nil, **options)
    unknown = options.keys - JOINT_OPTIONS
    raise ArgumentError, "unknown joint option #{unknown.first.inspect} (#{JOINT_OPTIONS.join(", ")})" if unknown.any?

    Joint.__new(__joint(kind, object, other, options), kind, object, other)
  end
end
