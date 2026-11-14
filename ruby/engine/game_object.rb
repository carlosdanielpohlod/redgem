# Ruby side of scene objects. Native methods (props, alive?, destroy, visible...) are defined by
# the engine; this file adds the conveniences built on them.
class GameObject
  # Instances are created by the engine (one per scene object), never with GameObject.new. A
  # GameObject keeps standing for its object only: once that is destroyed, every native method but
  # alive? and destroy raises, even after its slot (the id) is given to a new object. The name and
  # the tag stay readable.
  attr_reader :id, :name, :tag

  def transform
    @transform ||= Transform.__new(@id, @serial)
  end

  def character
    return nil unless __has_character?

    @character ||= CharacterController.__new(@id, @serial)
  end

  def rigidbody
    return nil unless __has_rigidbody?

    @rigidbody ||= Rigidbody.__new(@id, @serial)
  end

  def vehicle
    return nil unless __has_vehicle?

    @vehicle ||= Vehicle.__new(@id, @serial)
  end

  # The light it shines with (a lamp, a fire's glow), or nil when it has none.
  def light
    return nil unless __has_light?

    @light ||= Light.__new(@id, @serial)
  end

  # Shape keys (Blender's, glTF morph targets) of its mesh and of the meshes under it, by name:
  #   face.morph(:Smile, 0.7)  # sets the weight (0 off, 1 whole) on every part that has the key
  #   face.morph(:Smile)       # the weight, or nil when no part has that key
  #   face.morph_names         # => [:Smile, :Blink_L, ...]
  # A clip that animates the weights sets them again as it plays.
  def morph(name, weight = nil)
    return __morph_weight(name.to_s) if weight.nil?

    raise ArgumentError, "#{self.name} has no shape key #{name}" if __set_morph(name.to_s, Float(weight)).zero?

    weight
  end

  def tag?(value) = tag == value.to_s

  # Puts it under another object (nil: at the root of the scene), where it stays in the world: from
  # then on it moves with its new parent. An object cannot go under itself or what it carries.
  #   gun.parent = hand
  def parent=(parent)
    set_parent(parent)
  end

  # Like parent=; keep_world: false keeps its local transform instead, so it lands where it was
  # relative to its old parent, now relative to the new one. Returns the parent.
  def set_parent(parent, keep_world: true) = __set_parent(parent, keep_world)

  def behaviours = @behaviours.dup

  # The first behaviour of this object that is a `klass`.
  def get_component(klass) = @behaviours.find { |behaviour| behaviour.is_a?(klass) }

  # Puts a new behaviour of `klass` on this object and returns it. The
  # settings are what the behaviour reads with `prop`, ahead of the object's Blender properties:
  #   car.add_component(TrafficCar, speed: 12.0)
  # It starts before its next update, like a behaviour that came with the scene.
  def add_component(klass, **settings)
    behaviour = __add_component(klass)
    behaviour&.__configure(settings)
    behaviour
  end

  def ==(other) = other.is_a?(GameObject) && other.id == @id && other.__serial == @serial

  def __serial = @serial

  def inspect = "#<GameObject #{name.inspect} id=#{@id}>"
  alias to_s inspect

  def __add_behaviour(behaviour)
    @behaviours << behaviour
  end

  # The object was destroyed: what the prelude kept about it goes too.
  def __destroyed
    @behaviours.clear
    Network.__object_gone(self)
  end
end
