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

  def inspect = "#<GameObject #{name.inspect} id=#{@id}>"
  alias to_s inspect

  def __add_behaviour(behaviour)
    @behaviours << behaviour
  end

  def __remove_behaviours
    @behaviours.clear
  end
end

class Transform
  def self.__new(id)
    transform = allocate
    transform.instance_variable_set(:@id, id)
    transform
  end

  def translate(offset)
    self.position = position + offset
  end

  def inspect = "#<Transform position=#{position} rotation=#{rotation}>"
end

class CharacterController
  def self.__new(id)
    controller = allocate
    controller.instance_variable_set(:@id, id)
    controller
  end
end

class Rigidbody
  def self.__new(id)
    body = allocate
    body.instance_variable_set(:@id, id)
    body
  end
end

# A car built in Blender: `body = "vehicle"` on the chassis, `wheel = true` on its wheel children.
class Vehicle
  def self.__new(id)
    vehicle = allocate
    vehicle.instance_variable_set(:@id, id)
    vehicle
  end

  # throttle and steering in -1..1 (positive steering turns right), brakes in 0..1.
  # The input stays applied until the next call.
  def drive(throttle: 0.0, steering: 0.0, brake: 0.0, handbrake: 0.0)
    __drive(throttle.to_f, steering.to_f, brake.to_f, handbrake.to_f)
  end

  def stop = drive(brake: 1.0)
end

module Scene
  # Copies an object (usually a `prefab = true` template) into the running game, with its physics
  # and scripts. position: Vector3, rotation: Euler degrees (Vector3); both default to the template's.
  # physics: false leaves the copy without bodies: something to look at that nothing bumps into.
  def self.spawn(template, position: nil, rotation: nil, physics: true)
    __spawn(template, position, rotation, physics)
  end
end
