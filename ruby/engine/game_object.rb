# Ruby side of scene objects. Native methods (name, tag, props, alive?, destroy, visible...)
# are defined by the engine; this file adds the conveniences built on them.
class GameObject
  # Instances are created by the engine (one per scene object), never with GameObject.new.
  attr_reader :id

  def transform
    @transform ||= Transform.__new(@id)
  end

  def character
    return nil unless __has_character?

    @character ||= CharacterController.__new(@id)
  end

  def rigidbody
    return nil unless __has_rigidbody?

    @rigidbody ||= Rigidbody.__new(@id)
  end

  def vehicle
    return nil unless __has_vehicle?

    @vehicle ||= Vehicle.__new(@id)
  end

  def tag?(value) = tag == value.to_s

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

  def ==(other) = other.is_a?(GameObject) && other.id == @id

  # How it shows in the console and in errors: its name and id, not its insides (which point back
  # at it through its behaviours).
  def inspect = "#<GameObject #{name.inspect} id=#{@id}>"
  alias to_s inspect

  def __add_behaviour(behaviour)
    @behaviours << behaviour
  end

  def __remove_behaviours
    @behaviours.clear
  end
end
