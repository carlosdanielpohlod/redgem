# What an object may have besides its behaviours: where it is, and its body in the physics
# world. Instances are made by GameObject, never with new; the engine defines what they do.
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
